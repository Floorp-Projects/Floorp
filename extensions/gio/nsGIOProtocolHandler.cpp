/* vim:set ts=2 sw=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This code is based on original Mozilla gnome-vfs extension. It implements
 * input stream provided by GVFS/GIO.
*/
#include "mozilla/ModuleUtils.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIObserver.h"
#include "nsThreadUtils.h"
#include "nsProxyRelease.h"
#include "nsIStringBundle.h"
#include "nsIStandardURL.h"
#include "nsMimeTypes.h"
#include "nsNetUtil.h"
#include "nsNullPrincipal.h"
#include "mozilla/Monitor.h"
#include <gio/gio.h>
#include <algorithm>

#define MOZ_GIO_SCHEME              "moz-gio"
#define MOZ_GIO_SUPPORTED_PROTOCOLS "network.gio.supported-protocols"

//-----------------------------------------------------------------------------

// NSPR_LOG_MODULES=gio:5
#ifdef PR_LOGGING
static PRLogModuleInfo *sGIOLog;
#define LOG(args) PR_LOG(sGIOLog, PR_LOG_DEBUG, args)
#else
#define LOG(args)
#endif


//-----------------------------------------------------------------------------
static nsresult
MapGIOResult(gint code) 
{
  switch (code)
  {
     case G_IO_ERROR_NOT_FOUND:                  return NS_ERROR_FILE_NOT_FOUND; // shows error
     case G_IO_ERROR_INVALID_ARGUMENT:           return NS_ERROR_INVALID_ARG;
     case G_IO_ERROR_NOT_SUPPORTED:              return NS_ERROR_NOT_AVAILABLE;
     case G_IO_ERROR_NO_SPACE:                   return NS_ERROR_FILE_NO_DEVICE_SPACE;
     case G_IO_ERROR_READ_ONLY:                  return NS_ERROR_FILE_READ_ONLY;
     case G_IO_ERROR_PERMISSION_DENIED:          return NS_ERROR_FILE_ACCESS_DENIED; // wrong password/login
     case G_IO_ERROR_CLOSED:                     return NS_BASE_STREAM_CLOSED; // was EOF
     case G_IO_ERROR_NOT_DIRECTORY:              return NS_ERROR_FILE_NOT_DIRECTORY;
     case G_IO_ERROR_PENDING:                    return NS_ERROR_IN_PROGRESS;
     case G_IO_ERROR_EXISTS:                     return NS_ERROR_FILE_ALREADY_EXISTS;
     case G_IO_ERROR_IS_DIRECTORY:               return NS_ERROR_FILE_IS_DIRECTORY;
     case G_IO_ERROR_NOT_MOUNTED:                return NS_ERROR_NOT_CONNECTED; // shows error
     case G_IO_ERROR_HOST_NOT_FOUND:             return NS_ERROR_UNKNOWN_HOST; // shows error
     case G_IO_ERROR_CANCELLED:                  return NS_ERROR_ABORT;
     case G_IO_ERROR_NOT_EMPTY:                  return NS_ERROR_FILE_DIR_NOT_EMPTY;
     case G_IO_ERROR_FILENAME_TOO_LONG:          return NS_ERROR_FILE_NAME_TOO_LONG;
     case G_IO_ERROR_INVALID_FILENAME:           return NS_ERROR_FILE_INVALID_PATH;
     case G_IO_ERROR_TIMED_OUT:                  return NS_ERROR_NET_TIMEOUT; // shows error
     case G_IO_ERROR_WOULD_BLOCK:                return NS_BASE_STREAM_WOULD_BLOCK;
     case G_IO_ERROR_FAILED_HANDLED:             return NS_ERROR_ABORT; // Cancel on login dialog

/* unhandled:
  G_IO_ERROR_NOT_REGULAR_FILE,
  G_IO_ERROR_NOT_SYMBOLIC_LINK,
  G_IO_ERROR_NOT_MOUNTABLE_FILE,
  G_IO_ERROR_TOO_MANY_LINKS,
  G_IO_ERROR_ALREADY_MOUNTED,
  G_IO_ERROR_CANT_CREATE_BACKUP,
  G_IO_ERROR_WRONG_ETAG,
  G_IO_ERROR_WOULD_RECURSE,
  G_IO_ERROR_BUSY,
  G_IO_ERROR_WOULD_MERGE,
  G_IO_ERROR_TOO_MANY_OPEN_FILES
*/
    // Make GCC happy
    default:
      return NS_ERROR_FAILURE;
  }

  return NS_ERROR_FAILURE;
}

static nsresult
MapGIOResult(GError *result)
{
  if (!result)
    return NS_OK;
  else 
    return MapGIOResult(result->code);
}
/** Return values for mount operation.
 * These enums are used as mount operation return values.
 */
typedef enum {
  MOUNT_OPERATION_IN_PROGRESS, /** \enum operation in progress */
  MOUNT_OPERATION_SUCCESS,     /** \enum operation successful */
  MOUNT_OPERATION_FAILED       /** \enum operation not successful */
} MountOperationResult;
//-----------------------------------------------------------------------------
/**
 * Sort function compares according to file type (directory/file)
 * and alphabethical order
 * @param a pointer to GFileInfo object to compare
 * @param b pointer to GFileInfo object to compare
 * @return -1 when first object should be before the second, 0 when equal,
 * +1 when second object should be before the first
 */
static gint
FileInfoComparator(gconstpointer a, gconstpointer b)
{
  GFileInfo *ia = ( GFileInfo *) a;
  GFileInfo *ib = ( GFileInfo *) b;
  if (g_file_info_get_file_type(ia) == G_FILE_TYPE_DIRECTORY
      && g_file_info_get_file_type(ib) != G_FILE_TYPE_DIRECTORY)
    return -1;
  if (g_file_info_get_file_type(ib) == G_FILE_TYPE_DIRECTORY
      && g_file_info_get_file_type(ia) != G_FILE_TYPE_DIRECTORY)
    return 1;

  return strcasecmp(g_file_info_get_name(ia), g_file_info_get_name(ib));
}

/* Declaration of mount callback functions */
static void mount_enclosing_volume_finished (GObject *source_object,
                                             GAsyncResult *res,
                                             gpointer user_data);
static void mount_operation_ask_password (GMountOperation   *mount_op,
                                          const char        *message,
                                          const char        *default_user,
                                          const char        *default_domain,
                                          GAskPasswordFlags flags,
                                          gpointer          user_data);
//-----------------------------------------------------------------------------

class nsGIOInputStream MOZ_FINAL : public nsIInputStream
{
   ~nsGIOInputStream() { Close(); }

  public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIINPUTSTREAM

    explicit nsGIOInputStream(const nsCString &uriSpec)
      : mSpec(uriSpec)
      , mChannel(nullptr)
      , mHandle(nullptr)
      , mStream(nullptr)
      , mBytesRemaining(UINT64_MAX)
      , mStatus(NS_OK)
      , mDirList(nullptr)
      , mDirListPtr(nullptr)
      , mDirBufCursor(0)
      , mDirOpen(false)
      , mMonitorMountInProgress("GIOInputStream::MountFinished") { }

    void SetChannel(nsIChannel *channel)
    {
      // We need to hold an owning reference to our channel.  This is done
      // so we can access the channel's notification callbacks to acquire
      // a reference to a nsIAuthPrompt if we need to handle an interactive
      // mount operation.
      //
      // However, the channel can only be accessed on the main thread, so
      // we have to be very careful with ownership.  Moreover, it doesn't
      // support threadsafe addref/release, so proxying is the answer.
      //
      // Also, it's important to note that this likely creates a reference
      // cycle since the channel likely owns this stream.  This reference
      // cycle is broken in our Close method.

      NS_ADDREF(mChannel = channel);
    }
    void           SetMountResult(MountOperationResult result, gint error_code);
  private:
    nsresult       DoOpen();
    nsresult       DoRead(char *aBuf, uint32_t aCount, uint32_t *aCountRead);
    nsresult       SetContentTypeOfChannel(const char *contentType);
    nsresult       MountVolume();
    nsresult       DoOpenDirectory();
    nsresult       DoOpenFile(GFileInfo *info);        
    nsCString             mSpec;
    nsIChannel           *mChannel; // manually refcounted
    GFile                *mHandle;
    GFileInputStream     *mStream;
    uint64_t              mBytesRemaining;
    nsresult              mStatus;
    GList                *mDirList;
    GList                *mDirListPtr;
    nsCString             mDirBuf;
    uint32_t              mDirBufCursor;
    bool                  mDirOpen;
    MountOperationResult  mMountRes;
    mozilla::Monitor      mMonitorMountInProgress;
    gint                  mMountErrorCode;
};
/**
 * Set result of mount operation and notify monitor waiting for results.
 * This method is called in main thread as long as it is used only 
 * in mount_enclosing_volume_finished function.
 * @param result Result of mount operation
 */ 
void
nsGIOInputStream::SetMountResult(MountOperationResult result, gint error_code)
{
  mozilla::MonitorAutoLock mon(mMonitorMountInProgress);
  mMountRes = result;
  mMountErrorCode = error_code;
  mon.Notify();
}

/**
 * Start mount operation and wait in loop until it is finished. This method is 
 * called from thread which is trying to read from location.
 */
nsresult
nsGIOInputStream::MountVolume() {
  GMountOperation* mount_op = g_mount_operation_new();
  g_signal_connect (mount_op, "ask-password",
                    G_CALLBACK (mount_operation_ask_password), mChannel);
  mMountRes = MOUNT_OPERATION_IN_PROGRESS;
  /* g_file_mount_enclosing_volume uses a dbus request to mount the volume.
     Callback mount_enclosing_volume_finished is called in main thread 
     (not this thread on which this method is called). */
  g_file_mount_enclosing_volume(mHandle,
                                G_MOUNT_MOUNT_NONE,
                                mount_op,
                                nullptr,
                                mount_enclosing_volume_finished,
                                this);
  mozilla::MonitorAutoLock mon(mMonitorMountInProgress);
  /* Waiting for finish of mount operation thread */  
  while (mMountRes == MOUNT_OPERATION_IN_PROGRESS)
    mon.Wait();
  
  g_object_unref(mount_op);

  if (mMountRes == MOUNT_OPERATION_FAILED) {
    return MapGIOResult(mMountErrorCode);
  } else {
    return NS_OK;
  }
}

/**
 * Create list of infos about objects in opened directory
 * Return: NS_OK when list obtained, otherwise error code according
 * to failed operation.
 */
nsresult
nsGIOInputStream::DoOpenDirectory()
{
  GError *error = nullptr;

  GFileEnumerator *f_enum = g_file_enumerate_children(mHandle,
                                                      "standard::*,time::*",
                                                      G_FILE_QUERY_INFO_NONE,
                                                      nullptr,
                                                      &error);
  if (!f_enum) {
    nsresult rv = MapGIOResult(error);
    g_warning("Cannot read from directory: %s", error->message);
    g_error_free(error);
    return rv;
  }
  // fill list of file infos
  GFileInfo *info = g_file_enumerator_next_file(f_enum, nullptr, &error);
  while (info) {
    mDirList = g_list_append(mDirList, info);
    info = g_file_enumerator_next_file(f_enum, nullptr, &error);
  }
  g_object_unref(f_enum);
  if (error) {
    g_warning("Error reading directory content: %s", error->message);
    nsresult rv = MapGIOResult(error);
    g_error_free(error);
    return rv;
  }
  mDirOpen = true;

  // Sort list of file infos by using FileInfoComparator function
  mDirList = g_list_sort(mDirList, FileInfoComparator);
  mDirListPtr = mDirList;

  // Write base URL (make sure it ends with a '/')
  mDirBuf.AppendLiteral("300: ");
  mDirBuf.Append(mSpec);
  if (mSpec.get()[mSpec.Length() - 1] != '/')
    mDirBuf.Append('/');
  mDirBuf.Append('\n');

  // Write column names
  mDirBuf.AppendLiteral("200: filename content-length last-modified file-type\n");

  // Write charset (assume UTF-8)
  // XXX is this correct?
  mDirBuf.AppendLiteral("301: UTF-8\n");
  SetContentTypeOfChannel(APPLICATION_HTTP_INDEX_FORMAT);
  return NS_OK;
}

/**
 * Create file stream and set mime type for channel
 * @param info file info used to determine mime type
 * @return NS_OK when file stream created successfuly, error code otherwise
 */
nsresult
nsGIOInputStream::DoOpenFile(GFileInfo *info)
{
  GError *error = nullptr;

  mStream = g_file_read(mHandle, nullptr, &error);
  if (!mStream) {
    nsresult rv = MapGIOResult(error);
    g_warning("Cannot read from file: %s", error->message);
    g_error_free(error);
    return rv;
  }

  const char * content_type = g_file_info_get_content_type(info);
  if (content_type) {
    char *mime_type = g_content_type_get_mime_type(content_type);
    if (mime_type) {
      if (strcmp(mime_type, APPLICATION_OCTET_STREAM) != 0) {
        SetContentTypeOfChannel(mime_type);
      }
      g_free(mime_type);
    }
  } else {
    g_warning("Missing content type.");
  }

  mBytesRemaining = g_file_info_get_size(info);
  // Update the content length attribute on the channel.  We do this
  // synchronously without proxying.  This hack is not as bad as it looks!
  mChannel->SetContentLength(mBytesRemaining);

  return NS_OK;
}

/**
 * Start file open operation, mount volume when needed and according to file type
 * create file output stream or read directory content.
 * @return NS_OK when file or directory opened successfully, error code otherwise
 */
nsresult
nsGIOInputStream::DoOpen()
{
  nsresult rv;
  GError *error = nullptr;

  NS_ASSERTION(mHandle == nullptr, "already open");

  mHandle = g_file_new_for_uri( mSpec.get() );

  GFileInfo *info = g_file_query_info(mHandle,
                                      "standard::*",
                                      G_FILE_QUERY_INFO_NONE,
                                      nullptr,
                                      &error);

  if (error) {
    if (error->domain == G_IO_ERROR && error->code == G_IO_ERROR_NOT_MOUNTED) {
      // location is not yet mounted, try to mount
      g_error_free(error);
      if (NS_IsMainThread()) 
        return NS_ERROR_NOT_CONNECTED;
      error = nullptr;
      rv = MountVolume();
      if (rv != NS_OK) {
        return rv;
      }
      // get info again
      info = g_file_query_info(mHandle,
                               "standard::*",
                               G_FILE_QUERY_INFO_NONE,
                               nullptr,
                               &error);
      // second try to get file info from remote files after media mount
      if (!info) {
        g_warning("Unable to get file info: %s", error->message);
        rv = MapGIOResult(error);
        g_error_free(error);
        return rv;
      }
    } else {
      g_warning("Unable to get file info: %s", error->message);
      rv = MapGIOResult(error);
      g_error_free(error);
      return rv;
    }
  }
  // Get file type to handle directories and file differently
  GFileType f_type = g_file_info_get_file_type(info);
  if (f_type == G_FILE_TYPE_DIRECTORY) {
    // directory
    rv = DoOpenDirectory();
  } else if (f_type != G_FILE_TYPE_UNKNOWN) {
    // file
    rv = DoOpenFile(info);
  } else {
    g_warning("Unable to get file type.");
    rv = NS_ERROR_FILE_NOT_FOUND;
  }
  if (info)
    g_object_unref(info);
  return rv;
}

/**
 * Read content of file or create file list from directory
 * @param aBuf read destination buffer
 * @param aCount length of destination buffer
 * @param aCountRead number of read characters
 * @return NS_OK when read successfully, NS_BASE_STREAM_CLOSED when end of file,
 *         error code otherwise
 */
nsresult
nsGIOInputStream::DoRead(char *aBuf, uint32_t aCount, uint32_t *aCountRead)
{
  nsresult rv = NS_ERROR_NOT_AVAILABLE;
  if (mStream) {
    // file read
    GError *error = nullptr;    
    uint32_t bytes_read = g_input_stream_read(G_INPUT_STREAM(mStream),
                                              aBuf,
                                              aCount,
                                              nullptr,
                                              &error);
    if (error) {
      rv = MapGIOResult(error);
      *aCountRead = 0;
      g_warning("Cannot read from file: %s", error->message);
      g_error_free(error);
      return rv;
    }
    *aCountRead = bytes_read;
    mBytesRemaining -= *aCountRead;
    return NS_OK;
  }
  else if (mDirOpen) {
    // directory read
    while (aCount && rv != NS_BASE_STREAM_CLOSED)
    {
      // Copy data out of our buffer
      uint32_t bufLen = mDirBuf.Length() - mDirBufCursor;
      if (bufLen)
      {
        uint32_t n = std::min(bufLen, aCount);
        memcpy(aBuf, mDirBuf.get() + mDirBufCursor, n);
        *aCountRead += n;
        aBuf += n;
        aCount -= n;
        mDirBufCursor += n;
      }

      if (!mDirListPtr)    // Are we at the end of the directory list?
      {
        rv = NS_BASE_STREAM_CLOSED;
      }
      else if (aCount)     // Do we need more data?
      {
        GFileInfo *info = (GFileInfo *) mDirListPtr->data;

        // Prune '.' and '..' from directory listing.
        const char * fname = g_file_info_get_name(info);
        if (fname && fname[0] == '.' && 
            (fname[1] == '\0' || (fname[1] == '.' && fname[2] == '\0')))
        {
          mDirListPtr = mDirListPtr->next;
          continue;
        }

        mDirBuf.AssignLiteral("201: ");

        // The "filename" field
        nsCString escName;
        nsCOMPtr<nsINetUtil> nu = do_GetService(NS_NETUTIL_CONTRACTID);
        if (nu && fname) {
          nu->EscapeString(nsDependentCString(fname),
                           nsINetUtil::ESCAPE_URL_PATH, escName);

          mDirBuf.Append(escName);
          mDirBuf.Append(' ');
        }

        // The "content-length" field
        // XXX truncates size from 64-bit to 32-bit
        mDirBuf.AppendInt(int32_t(g_file_info_get_size(info)));
        mDirBuf.Append(' ');

        // The "last-modified" field
        //
        // NSPR promises: PRTime is compatible with time_t
        // we just need to convert from seconds to microseconds
        GTimeVal gtime;
        g_file_info_get_modification_time(info, &gtime);

        PRExplodedTime tm;
        PRTime pt = ((PRTime) gtime.tv_sec) * 1000000;
        PR_ExplodeTime(pt, PR_GMTParameters, &tm);
        {
          char buf[64];
          PR_FormatTimeUSEnglish(buf, sizeof(buf),
              "%a,%%20%d%%20%b%%20%Y%%20%H:%M:%S%%20GMT ", &tm);
          mDirBuf.Append(buf);
        }

        // The "file-type" field
        switch (g_file_info_get_file_type(info))
        {
          case G_FILE_TYPE_REGULAR:
            mDirBuf.AppendLiteral("FILE ");
            break;
          case G_FILE_TYPE_DIRECTORY:
            mDirBuf.AppendLiteral("DIRECTORY ");
            break;
          case G_FILE_TYPE_SYMBOLIC_LINK:
            mDirBuf.AppendLiteral("SYMBOLIC-LINK ");
            break;
          default:
            break;
        }
        mDirBuf.Append('\n');

        mDirBufCursor = 0;
        mDirListPtr = mDirListPtr->next;
      }
    }
  }
  return rv;
}

/**
 * This class is used to implement SetContentTypeOfChannel.
 */
class nsGIOSetContentTypeEvent : public nsRunnable
{
  public:
    nsGIOSetContentTypeEvent(nsIChannel *channel, const char *contentType)
      : mChannel(channel), mContentType(contentType)
    {
      // stash channel reference in mChannel.  no AddRef here!  see note
      // in SetContentTypeOfchannel.
    }

    NS_IMETHOD Run()
    {
      mChannel->SetContentType(mContentType);
      return NS_OK;
    }

  private:
    nsIChannel *mChannel;
    nsCString   mContentType;
};

nsresult
nsGIOInputStream::SetContentTypeOfChannel(const char *contentType)
{
  // We need to proxy this call over to the main thread.  We post an
  // asynchronous event in this case so that we don't delay reading data, and
  // we know that this is safe to do since the channel's reference will be
  // released asynchronously as well.  We trust the ordering of the main
  // thread's event queue to protect us against memory corruption.

  nsresult rv;
  nsCOMPtr<nsIRunnable> ev =
      new nsGIOSetContentTypeEvent(mChannel, contentType);
  if (!ev)
  {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  else
  {
    rv = NS_DispatchToMainThread(ev);
  }
  return rv;
}

NS_IMPL_ISUPPORTS(nsGIOInputStream, nsIInputStream)

/**
 * Free all used memory and close stream.
 */
NS_IMETHODIMP
nsGIOInputStream::Close()
{
  if (mStream)
  {
    g_object_unref(mStream);
    mStream = nullptr;
  }

  if (mHandle)
  {
    g_object_unref(mHandle);
    mHandle = nullptr;
  }

  if (mDirList)
  {
    // Destroy the list of GIOFileInfo objects...
    g_list_foreach(mDirList, (GFunc) g_object_unref, nullptr);
    g_list_free(mDirList);
    mDirList = nullptr;
    mDirListPtr = nullptr;
  }

  if (mChannel)
  {
    nsresult rv = NS_OK;

    nsCOMPtr<nsIThread> thread = do_GetMainThread();
    if (thread)
      rv = NS_ProxyRelease(thread, mChannel);

    NS_ASSERTION(thread && NS_SUCCEEDED(rv), "leaking channel reference");
    mChannel = nullptr;
    (void) rv;
  }

  mSpec.Truncate(); // free memory

  // Prevent future reads from re-opening the handle.
  if (NS_SUCCEEDED(mStatus))
    mStatus = NS_BASE_STREAM_CLOSED;

  return NS_OK;
}

/**
 * Return number of remaining bytes available on input
 * @param aResult remaining bytes
 */
NS_IMETHODIMP
nsGIOInputStream::Available(uint64_t *aResult)
{
  if (NS_FAILED(mStatus))
    return mStatus;

  *aResult = mBytesRemaining;

  return NS_OK;
}

/**
 * Trying to read from stream. When location is not available it tries to mount it.
 * @param aBuf buffer to put read data
 * @param aCount length of aBuf
 * @param aCountRead number of bytes actually read
 */
NS_IMETHODIMP
nsGIOInputStream::Read(char     *aBuf,
                       uint32_t  aCount,
                       uint32_t *aCountRead)
{
  *aCountRead = 0;
  // Check if file is already opened, otherwise open it
  if (!mStream && !mDirOpen && mStatus == NS_OK) {
    mStatus = DoOpen();
    if (NS_FAILED(mStatus)) {
      return mStatus;
    }
  }

  mStatus = DoRead(aBuf, aCount, aCountRead);
  // Check if all data has been read
  if (mStatus == NS_BASE_STREAM_CLOSED)
    return NS_OK;

  // Check whenever any error appears while reading
  return mStatus;
}

NS_IMETHODIMP
nsGIOInputStream::ReadSegments(nsWriteSegmentFun aWriter,
                               void             *aClosure,
                               uint32_t          aCount,
                               uint32_t         *aResult)
{
  // There is no way to implement this using GnomeVFS, but fortunately
  // that doesn't matter.  Because we are a blocking input stream, Necko
  // isn't going to call our ReadSegments method.
  NS_NOTREACHED("nsGIOInputStream::ReadSegments");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsGIOInputStream::IsNonBlocking(bool *aResult)
{
  *aResult = false;
  return NS_OK;
}

//-----------------------------------------------------------------------------

/**
 * Called when finishing mount operation. Result of operation is set in 
 * nsGIOInputStream. This function is called in main thread as an async request 
 * typically from dbus.
 * @param source_object GFile object which requested the mount
 * @param res result object
 * @param user_data pointer to nsGIOInputStream
 */
static void
mount_enclosing_volume_finished (GObject *source_object,
                                 GAsyncResult *res,
                                 gpointer user_data)
{
  GError *error = nullptr;

  nsGIOInputStream* istream = static_cast<nsGIOInputStream*>(user_data);
  
  g_file_mount_enclosing_volume_finish(G_FILE (source_object), res, &error);
  
  if (error) {
    g_warning("Mount failed: %s %d", error->message, error->code);
    istream->SetMountResult(MOUNT_OPERATION_FAILED, error->code);
    g_error_free(error);
  } else {
    istream->SetMountResult(MOUNT_OPERATION_SUCCESS, 0);
  }
}

/**
 * This function is called when username or password are requested from user.
 * This function is called in main thread as async request from dbus.
 * @param mount_op mount operation
 * @param message message to show to user
 * @param default_user preffered user
 * @param default_domain domain name
 * @param flags what type of information is required
 * @param user_data nsIChannel
 */
static void
mount_operation_ask_password (GMountOperation   *mount_op,
                              const char        *message,
                              const char        *default_user,
                              const char        *default_domain,
                              GAskPasswordFlags flags,
                              gpointer          user_data)
{
  nsIChannel *channel = (nsIChannel *) user_data;
  if (!channel) {
    g_mount_operation_reply(mount_op, G_MOUNT_OPERATION_ABORTED);
    return;
  }
  // We can't handle request for domain
  if (flags & G_ASK_PASSWORD_NEED_DOMAIN) {
    g_mount_operation_reply(mount_op, G_MOUNT_OPERATION_ABORTED);
    return;
  }

  nsCOMPtr<nsIAuthPrompt> prompt;
  NS_QueryNotificationCallbacks(channel, prompt);

  // If no auth prompt, then give up.  We could failover to using the
  // WindowWatcher service, but that might defeat a consumer's purposeful
  // attempt to disable authentication (for whatever reason).
  if (!prompt) {
    g_mount_operation_reply(mount_op, G_MOUNT_OPERATION_ABORTED);
    return;
  }
  // Parse out the host and port...
  nsCOMPtr<nsIURI> uri;
  channel->GetURI(getter_AddRefs(uri));
  if (!uri) {
    g_mount_operation_reply(mount_op, G_MOUNT_OPERATION_ABORTED);
    return;
  }

  nsAutoCString scheme, hostPort;
  uri->GetScheme(scheme);
  uri->GetHostPort(hostPort);

  // It doesn't make sense for either of these strings to be empty.  What kind
  // of funky URI is this?
  if (scheme.IsEmpty() || hostPort.IsEmpty()) {
    g_mount_operation_reply(mount_op, G_MOUNT_OPERATION_ABORTED);
    return;
  }
  // Construct the single signon key.  Altering the value of this key will
  // cause people's remembered passwords to be forgotten.  Think carefully
  // before changing the way this key is constructed.
  nsAutoString key, realm;

  NS_ConvertUTF8toUTF16 dispHost(scheme);
  dispHost.AppendLiteral("://");
  dispHost.Append(NS_ConvertUTF8toUTF16(hostPort));

  key = dispHost;
  if (*default_domain != '\0')
  {
    // We assume the realm string is ASCII.  That might be a bogus assumption,
    // but we have no idea what encoding GnomeVFS is using, so for now we'll
    // limit ourselves to ISO-Latin-1.  XXX What is a better solution?
    realm.Append('"');
    realm.Append(NS_ConvertASCIItoUTF16(default_domain));
    realm.Append('"');
    key.Append(' ');
    key.Append(realm);
  }
  // Construct the message string...
  //
  // We use Necko's string bundle here.  This code really should be encapsulated
  // behind some Necko API, after all this code is based closely on the code in
  // nsHttpChannel.cpp.
  nsCOMPtr<nsIStringBundleService> bundleSvc =
      do_GetService(NS_STRINGBUNDLE_CONTRACTID);
  if (!bundleSvc) {
    g_mount_operation_reply(mount_op, G_MOUNT_OPERATION_ABORTED);
    return;
  }
  nsCOMPtr<nsIStringBundle> bundle;
  bundleSvc->CreateBundle("chrome://global/locale/commonDialogs.properties",
                          getter_AddRefs(bundle));
  if (!bundle) {
    g_mount_operation_reply(mount_op, G_MOUNT_OPERATION_ABORTED);
    return;
  }
  nsAutoString nsmessage;

  if (flags & G_ASK_PASSWORD_NEED_PASSWORD) {
    if (flags & G_ASK_PASSWORD_NEED_USERNAME) {
      if (!realm.IsEmpty()) {
        const char16_t *strings[] = { realm.get(), dispHost.get() };
        bundle->FormatStringFromName(MOZ_UTF16("EnterLoginForRealm"),
                                     strings, 2, getter_Copies(nsmessage));
      } else {
        const char16_t *strings[] = { dispHost.get() };
        bundle->FormatStringFromName(MOZ_UTF16("EnterUserPasswordFor"),
                                     strings, 1, getter_Copies(nsmessage));
      }
    } else {
      NS_ConvertUTF8toUTF16 userName(default_user);
      const char16_t *strings[] = { userName.get(), dispHost.get() };
      bundle->FormatStringFromName(MOZ_UTF16("EnterPasswordFor"),
                                   strings, 2, getter_Copies(nsmessage));
    }
  } else {
    g_warning("Unknown mount operation request (flags: %x)", flags);
  }

  if (nsmessage.IsEmpty()) {
    g_mount_operation_reply(mount_op, G_MOUNT_OPERATION_ABORTED);
    return;
  }
  // Prompt the user...
  nsresult rv;
  bool retval = false;
  char16_t *user = nullptr, *pass = nullptr;
  if (default_user) {
    // user will be freed by PromptUsernameAndPassword
    user = ToNewUnicode(NS_ConvertUTF8toUTF16(default_user));
  }
  if (flags & G_ASK_PASSWORD_NEED_USERNAME) {
    rv = prompt->PromptUsernameAndPassword(nullptr, nsmessage.get(),
                                           key.get(),
                                           nsIAuthPrompt::SAVE_PASSWORD_PERMANENTLY,
                                           &user, &pass, &retval);
  } else {
    rv = prompt->PromptPassword(nullptr, nsmessage.get(),
                                key.get(),
                                nsIAuthPrompt::SAVE_PASSWORD_PERMANENTLY,
                                &pass, &retval);
  }
  if (NS_FAILED(rv) || !retval) {  //  was || user == '\0' || pass == '\0'
    g_mount_operation_reply(mount_op, G_MOUNT_OPERATION_ABORTED);
    return;
  }
  /* GIO should accept UTF8 */
  g_mount_operation_set_username(mount_op, NS_ConvertUTF16toUTF8(user).get());
  g_mount_operation_set_password(mount_op, NS_ConvertUTF16toUTF8(pass).get());
  nsMemory::Free(user);
  nsMemory::Free(pass);
  g_mount_operation_reply(mount_op, G_MOUNT_OPERATION_HANDLED);
}

//-----------------------------------------------------------------------------

class nsGIOProtocolHandler MOZ_FINAL : public nsIProtocolHandler
                                     , public nsIObserver
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROTOCOLHANDLER
    NS_DECL_NSIOBSERVER

    nsresult Init();

  private:
    ~nsGIOProtocolHandler() {}

    void InitSupportedProtocolsPref(nsIPrefBranch *prefs);
    bool IsSupportedProtocol(const nsCString &spec);

    nsCString mSupportedProtocols;
};

NS_IMPL_ISUPPORTS(nsGIOProtocolHandler, nsIProtocolHandler, nsIObserver)

nsresult
nsGIOProtocolHandler::Init()
{
#ifdef PR_LOGGING
  sGIOLog = PR_NewLogModule("gio");
#endif

  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs)
  {
    InitSupportedProtocolsPref(prefs);
    prefs->AddObserver(MOZ_GIO_SUPPORTED_PROTOCOLS, this, false);
  }

  return NS_OK;
}

void
nsGIOProtocolHandler::InitSupportedProtocolsPref(nsIPrefBranch *prefs)
{
  // Get user preferences to determine which protocol is supported.
  // Gvfs/GIO has a set of supported protocols like obex, network, archive,
  // computer, dav, cdda, gphoto2, trash, etc. Some of these seems to be
  // irrelevant to process by browser. By default accept only smb and sftp
  // protocols so far.
  nsresult rv = prefs->GetCharPref(MOZ_GIO_SUPPORTED_PROTOCOLS,
                                   getter_Copies(mSupportedProtocols));
  if (NS_SUCCEEDED(rv)) {
    mSupportedProtocols.StripWhitespace();
    ToLowerCase(mSupportedProtocols);
  }
  else
    mSupportedProtocols.AssignLiteral("smb:,sftp:"); // use defaults

  LOG(("gio: supported protocols \"%s\"\n", mSupportedProtocols.get()));
}

bool
nsGIOProtocolHandler::IsSupportedProtocol(const nsCString &aSpec)
{
  const char *specString = aSpec.get();
  const char *colon = strchr(specString, ':');
  if (!colon)
    return false;

  uint32_t length = colon - specString + 1;

  // <scheme> + ':'
  nsCString scheme(specString, length);

  char *found = PL_strcasestr(mSupportedProtocols.get(), scheme.get());
  if (!found)
    return false;

  if (found[length] != ',' && found[length] != '\0')
    return false;

  return true;
}

NS_IMETHODIMP
nsGIOProtocolHandler::GetScheme(nsACString &aScheme)
{
  aScheme.Assign(MOZ_GIO_SCHEME);
  return NS_OK;
}

NS_IMETHODIMP
nsGIOProtocolHandler::GetDefaultPort(int32_t *aDefaultPort)
{
  *aDefaultPort = -1;
  return NS_OK;
}

NS_IMETHODIMP
nsGIOProtocolHandler::GetProtocolFlags(uint32_t *aProtocolFlags)
{
  // Is URI_STD true of all GnomeVFS URI types?
  *aProtocolFlags = URI_STD | URI_DANGEROUS_TO_LOAD;
  return NS_OK;
}

NS_IMETHODIMP
nsGIOProtocolHandler::NewURI(const nsACString &aSpec,
                             const char *aOriginCharset,
                             nsIURI *aBaseURI,
                             nsIURI **aResult)
{
  const nsCString flatSpec(aSpec);
  LOG(("gio: NewURI [spec=%s]\n", flatSpec.get()));

  if (!aBaseURI)
  {
    // XXX Is it good to support all GIO protocols?
    if (!IsSupportedProtocol(flatSpec))
      return NS_ERROR_UNKNOWN_PROTOCOL;

    int32_t colon_location = flatSpec.FindChar(':');
    if (colon_location <= 0)
      return NS_ERROR_UNKNOWN_PROTOCOL;

    // Verify that GIO supports this URI scheme.
    bool uri_scheme_supported = false;

    GVfs *gvfs = g_vfs_get_default();

    if (!gvfs) {
      g_warning("Cannot get GVfs object.");
      return NS_ERROR_UNKNOWN_PROTOCOL;
    }

    const gchar* const * uri_schemes = g_vfs_get_supported_uri_schemes(gvfs);

    while (*uri_schemes != nullptr) {
      // While flatSpec ends with ':' the uri_scheme does not. Therefore do not
      // compare last character.
      if (StringHead(flatSpec, colon_location).Equals(*uri_schemes)) {
        uri_scheme_supported = true;
        break;
      }
      uri_schemes++;
    }

    if (!uri_scheme_supported) {
      return NS_ERROR_UNKNOWN_PROTOCOL;
    }
  }

  nsresult rv;
  nsCOMPtr<nsIStandardURL> url =
      do_CreateInstance(NS_STANDARDURL_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return rv;

  rv = url->Init(nsIStandardURL::URLTYPE_STANDARD, -1, flatSpec,
                 aOriginCharset, aBaseURI);
  if (NS_SUCCEEDED(rv))
    rv = CallQueryInterface(url, aResult);
  return rv;

}

NS_IMETHODIMP
nsGIOProtocolHandler::NewChannel(nsIURI *aURI, nsIChannel **aResult)
{
  NS_ENSURE_ARG_POINTER(aURI);
  nsresult rv;

  nsAutoCString spec;
  rv = aURI->GetSpec(spec);
  if (NS_FAILED(rv))
    return rv;

  nsRefPtr<nsGIOInputStream> stream = new nsGIOInputStream(spec);
  if (!stream)
  {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  else
  {
    nsCOMPtr<nsIPrincipal> nullPrincipal =
      do_CreateInstance("@mozilla.org/nullprincipal;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // start out assuming an unknown content-type.  we'll set the content-type
    // to something better once we open the URI.
    rv = NS_NewInputStreamChannel(aResult,
                                  aURI,
                                  stream,
                                  nullPrincipal,
                                  nsILoadInfo::SEC_NORMAL,
                                  nsIContentPolicy::TYPE_OTHER,
                                  NS_LITERAL_CSTRING(UNKNOWN_CONTENT_TYPE));
    if (NS_SUCCEEDED(rv))
      stream->SetChannel(*aResult);
  }
  return rv;
}

NS_IMETHODIMP
nsGIOProtocolHandler::AllowPort(int32_t aPort,
                                const char *aScheme,
                                bool *aResult)
{
  // Don't override anything.
  *aResult = false;
  return NS_OK;
}

NS_IMETHODIMP
nsGIOProtocolHandler::Observe(nsISupports *aSubject,
                              const char *aTopic,
                              const char16_t *aData)
{
  if (strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID) == 0) {
    nsCOMPtr<nsIPrefBranch> prefs = do_QueryInterface(aSubject);
    InitSupportedProtocolsPref(prefs);
  }
  return NS_OK;
}

//-----------------------------------------------------------------------------

#define NS_GIOPROTOCOLHANDLER_CID                    \
{ /* ee706783-3af8-4d19-9e84-e2ebfe213480 */         \
    0xee706783,                                      \
    0x3af8,                                          \
    0x4d19,                                          \
    {0x9e, 0x84, 0xe2, 0xeb, 0xfe, 0x21, 0x34, 0x80} \
}

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsGIOProtocolHandler, Init)
NS_DEFINE_NAMED_CID(NS_GIOPROTOCOLHANDLER_CID);

static const mozilla::Module::CIDEntry kVFSCIDs[] = {
  { &kNS_GIOPROTOCOLHANDLER_CID, false, nullptr, nsGIOProtocolHandlerConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kVFSContracts[] = {
  { NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX MOZ_GIO_SCHEME, &kNS_GIOPROTOCOLHANDLER_CID },
  { nullptr }
};

static const mozilla::Module kVFSModule = {
  mozilla::Module::kVersion,
  kVFSCIDs,
  kVFSContracts
};

NSMODULE_DEFN(nsGIOModule) = &kVFSModule;
