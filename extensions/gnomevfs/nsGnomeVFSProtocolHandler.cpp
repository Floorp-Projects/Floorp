/* vim:set ts=2 sw=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla gnome-vfs extension.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by IBM Corporation are Copyright (C) 2004
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// GnomeVFS v2.2.2 is missing G_BEGIN_DECLS in gnome-vfs-module-callback.h
extern "C" {
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-standard-callbacks.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>
}

#include "nsIGenericFactory.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIPrefService.h"
#include "nsIPrefBranchInternal.h"
#include "nsIObserver.h"
#include "nsEventQueueUtils.h"
#include "nsProxyRelease.h"
#include "nsIAuthPrompt.h"
#include "nsIStringBundle.h"
#include "nsIStandardURL.h"
#include "nsIURL.h"
#include "nsMimeTypes.h"
#include "nsNetUtil.h"
#include "nsAutoPtr.h"
#include "nsError.h"
#include "nsEscape.h"
#include "nsCRT.h"
#include "netCore.h"
#include "prlog.h"
#include "prtime.h"

#define MOZ_GNOMEVFS_SCHEME              "moz-gnomevfs"
#define MOZ_GNOMEVFS_SUPPORTED_PROTOCOLS "network.gnomevfs.supported-protocols"

//-----------------------------------------------------------------------------

// NSPR_LOG_MODULES=gnomevfs:5
#ifdef PR_LOGGING
static PRLogModuleInfo *sGnomeVFSLog;
#define LOG(args) PR_LOG(sGnomeVFSLog, PR_LOG_DEBUG, args)
#else
#define LOG(args)
#endif

//-----------------------------------------------------------------------------

static nsresult
MapGnomeVFSResult(GnomeVFSResult result)
{
  switch (result)
  {
    case GNOME_VFS_OK:                           return NS_OK;
    case GNOME_VFS_ERROR_NOT_FOUND:              return NS_ERROR_FILE_NOT_FOUND;
    case GNOME_VFS_ERROR_INTERNAL:               return NS_ERROR_UNEXPECTED;
    case GNOME_VFS_ERROR_BAD_PARAMETERS:         return NS_ERROR_INVALID_ARG;
    case GNOME_VFS_ERROR_NOT_SUPPORTED:          return NS_ERROR_NOT_AVAILABLE;
    case GNOME_VFS_ERROR_CORRUPTED_DATA:         return NS_ERROR_FILE_CORRUPTED;
    case GNOME_VFS_ERROR_TOO_BIG:                return NS_ERROR_FILE_TOO_BIG;
    case GNOME_VFS_ERROR_NO_SPACE:               return NS_ERROR_FILE_NO_DEVICE_SPACE;
    case GNOME_VFS_ERROR_READ_ONLY:
    case GNOME_VFS_ERROR_READ_ONLY_FILE_SYSTEM:  return NS_ERROR_FILE_READ_ONLY;
    case GNOME_VFS_ERROR_INVALID_URI:
    case GNOME_VFS_ERROR_INVALID_HOST_NAME:      return NS_ERROR_MALFORMED_URI;
    case GNOME_VFS_ERROR_ACCESS_DENIED:
    case GNOME_VFS_ERROR_NOT_PERMITTED:
    case GNOME_VFS_ERROR_LOGIN_FAILED:           return NS_ERROR_FILE_ACCESS_DENIED;
    case GNOME_VFS_ERROR_EOF:                    return NS_BASE_STREAM_CLOSED;
    case GNOME_VFS_ERROR_NOT_A_DIRECTORY:        return NS_ERROR_FILE_NOT_DIRECTORY;
    case GNOME_VFS_ERROR_IN_PROGRESS:            return NS_ERROR_IN_PROGRESS;
    case GNOME_VFS_ERROR_FILE_EXISTS:            return NS_ERROR_FILE_ALREADY_EXISTS;
    case GNOME_VFS_ERROR_IS_DIRECTORY:           return NS_ERROR_FILE_IS_DIRECTORY;
    case GNOME_VFS_ERROR_NO_MEMORY:              return NS_ERROR_OUT_OF_MEMORY;
    case GNOME_VFS_ERROR_HOST_NOT_FOUND:
    case GNOME_VFS_ERROR_HOST_HAS_NO_ADDRESS:    return NS_ERROR_UNKNOWN_HOST;
    case GNOME_VFS_ERROR_CANCELLED:
    case GNOME_VFS_ERROR_INTERRUPTED:            return NS_ERROR_ABORT;
    case GNOME_VFS_ERROR_DIRECTORY_NOT_EMPTY:    return NS_ERROR_FILE_DIR_NOT_EMPTY;
    case GNOME_VFS_ERROR_NAME_TOO_LONG:          return NS_ERROR_FILE_NAME_TOO_LONG;
    case GNOME_VFS_ERROR_SERVICE_NOT_AVAILABLE:  return NS_ERROR_UNKNOWN_PROTOCOL;

    /* No special mapping for these error codes...

    case GNOME_VFS_ERROR_GENERIC:
    case GNOME_VFS_ERROR_IO:
    case GNOME_VFS_ERROR_WRONG_FORMAT:
    case GNOME_VFS_ERROR_BAD_FILE:
    case GNOME_VFS_ERROR_NOT_OPEN:
    case GNOME_VFS_ERROR_INVALID_OPEN_MODE:
    case GNOME_VFS_ERROR_TOO_MANY_OPEN_FILES:
    case GNOME_VFS_ERROR_LOOP:
    case GNOME_VFS_ERROR_DIRECTORY_BUSY:
    case GNOME_VFS_ERROR_TOO_MANY_LINKS:
    case GNOME_VFS_ERROR_NOT_SAME_FILE_SYSTEM:
    case GNOME_VFS_ERROR_SERVICE_OBSOLETE:
    case GNOME_VFS_ERROR_PROTOCOL_ERROR:
    case GNOME_VFS_ERROR_NO_MASTER_BROWSER:

    */

    // Make GCC happy
    default:
      return NS_ERROR_FAILURE;
  }

  return NS_ERROR_FAILURE;
}

//-----------------------------------------------------------------------------

// Query the channel for a nsIAuthPrompt reference (use the canonical method of
// searching the channel's notification callbacks first, and its loadgroup's
// callbacks seconds).  XXX This should really live somewhere else.
static void
GetAuthPromptFromChannel(nsIChannel *channel, nsIAuthPrompt **result)
{
  nsCOMPtr<nsIInterfaceRequestor> cbs;
  channel->GetNotificationCallbacks(getter_AddRefs(cbs));
  if (cbs)
    CallGetInterface(cbs.get(), result);

  if (!*result)
  {
    nsCOMPtr<nsILoadGroup> loadGroup;
    channel->GetLoadGroup(getter_AddRefs(loadGroup));
    if (loadGroup)
    {
      loadGroup->GetNotificationCallbacks(getter_AddRefs(cbs));
      if (cbs)
        CallGetInterface(cbs.get(), result);
    }
  }
}

static void
ProxiedAuthCallback(gconstpointer in,
                    gsize         in_size,
                    gpointer      out,
                    gsize         out_size,
                    gpointer      callback_data)
{
  GnomeVFSModuleCallbackAuthenticationIn *authIn =
      (GnomeVFSModuleCallbackAuthenticationIn *) in;
  GnomeVFSModuleCallbackAuthenticationOut *authOut =
      (GnomeVFSModuleCallbackAuthenticationOut *) out;

  LOG(("gnomevfs: ProxiedAuthCallback [uri=%s]\n", authIn->uri));

  // Without a channel, we have no way of getting a prompter.
  nsIChannel *channel = (nsIChannel *) callback_data;
  if (!channel)
    return;

  nsCOMPtr<nsIAuthPrompt> prompt;
  GetAuthPromptFromChannel(channel, getter_AddRefs(prompt));

  // If no auth prompt, then give up.  We could failover to using the
  // WindowWatcher service, but that might defeat a consumer's purposeful
  // attempt to disable authentication (for whatever reason).
  if (!prompt)
    return;

  // Parse out the host and port...
  nsCOMPtr<nsIURI> uri;
  channel->GetURI(getter_AddRefs(uri));
  if (!uri)
    return;

#ifdef DEBUG
  {
    //
    // Make sure authIn->uri is consistent with the channel's URI.
    //
    // XXX This check is probably not IDN safe, and it might incorrectly
    //     fire as a result of escaping differences.  It's unclear what
    //     kind of transforms GnomeVFS might have applied to the URI spec
    //     that we originally gave to it.  In spite of the likelihood of
    //     false hits, this check is probably still valuable.
    //
    nsCAutoString spec;
    uri->GetSpec(spec);
    int uriLen = strlen(authIn->uri);
    if (!StringHead(spec, uriLen).Equals(nsDependentCString(authIn->uri, uriLen)))
    {
      LOG(("gnomevfs: [spec=%s authIn->uri=%s]\n", spec.get(), authIn->uri));
      NS_ERROR("URI mismatch");
    }
  }
#endif

  nsCAutoString scheme, hostPort;
  uri->GetScheme(scheme);
  uri->GetHostPort(hostPort);

  // It doesn't make sense for either of these strings to be empty.  What kind
  // of funky URI is this?
  if (scheme.IsEmpty() || hostPort.IsEmpty())
    return;

  // Construct the single signon key.  Altering the value of this key will
  // cause peoples remembered passwords to be forgotten.  Think carefully
  // before changing the way this key is constructed.
  nsAutoString key, dispHost, realm;
  AppendUTF8toUTF16(scheme + NS_LITERAL_CSTRING("://") + hostPort, dispHost);
  key = dispHost;
  if (authIn->realm)
  {
    // We assume the realm string is ASCII.  That might be a bogus assumption,
    // but we have no idea what encoding GnomeVFS is using, so for now we'll
    // limit ourselves to ISO-Latin-1.  XXX What is a better solution?
    realm.Append(PRUnichar('"'));
    realm.AppendWithConversion(authIn->realm);
    realm.Append(PRUnichar('"'));
    key += NS_LITERAL_STRING(" ") + realm;
  }

  // Construct the message string...
  //
  // We use Necko's string bundle here.  This code really should be encapsulated
  // behind some Necko API, after all this code is based closely on the code in
  // nsHttpChannel.cpp.

  nsCOMPtr<nsIStringBundleService> bundleSvc =
      do_GetService(NS_STRINGBUNDLE_CONTRACTID);
  if (!bundleSvc)
    return;

  nsCOMPtr<nsIStringBundle> bundle;
  bundleSvc->CreateBundle(NECKO_MSGS_URL, getter_AddRefs(bundle));
  if (!bundle)
    return;

  nsXPIDLString message;
  if (!realm.IsEmpty())
  {
    const PRUnichar *strings[] = { realm.get(), dispHost.get() };
    bundle->FormatStringFromName(NS_LITERAL_STRING("EnterUserPasswordForRealm").get(),
                                 strings, 2, getter_Copies(message));
  }
  else
  {
    const PRUnichar *strings[] = { dispHost.get() };
    bundle->FormatStringFromName(NS_LITERAL_STRING("EnterUserPasswordFor").get(),
                                 strings, 1, getter_Copies(message));
  }
  if (!message)
    return;

  // Prompt the user...
  nsresult rv;
  PRBool retval = PR_FALSE;
  PRUnichar *user = nsnull, *pass = nsnull;

  rv = prompt->PromptUsernameAndPassword(nsnull, message.get(),
                                         key.get(),
                                         nsIAuthPrompt::SAVE_PASSWORD_PERMANENTLY,
                                         &user, &pass, &retval);
  if (NS_FAILED(rv))
    return;
  if (!retval || !user || !pass)
    return;

  // XXX We need to convert the UTF-16 username and password from our dialog to
  // strings that GnomeVFS can understand.  It's unclear what encoding GnomeVFS
  // expects, so for now we assume 7-bit ASCII.  Hopefully, we can get a better
  // solution at some point.

  // One copy is never enough...
  authOut->username = g_strdup(NS_LossyConvertUTF16toASCII(user).get());
  authOut->password = g_strdup(NS_LossyConvertUTF16toASCII(pass).get());

  nsMemory::Free(user);
  nsMemory::Free(pass);
}

struct nsGnomeVFSAuthParams
{
  gconstpointer in;
  gsize         in_size;
  gpointer      out;
  gsize         out_size;
  gpointer      callback_data;
};

PR_STATIC_CALLBACK(void *)
AuthCallbackEventHandler(PLEvent *ev)
{
  nsGnomeVFSAuthParams *params = (nsGnomeVFSAuthParams *) ev->owner;
  ProxiedAuthCallback(params->in, params->in_size,
                      params->out, params->out_size,
                      params->callback_data);
  return nsnull;
}

PR_STATIC_CALLBACK(void)
AuthCallbackEventDestructor(PLEvent *ev)
{
  // ignored
}

static void
AuthCallback(gconstpointer in,
             gsize         in_size,
             gpointer      out,
             gsize         out_size,
             gpointer      callback_data)
{
  // Need to proxy this callback over to the main thread.  This code is greatly
  // simplified by the fact that we are making a synchronous callback.  E.g., we
  // don't need to allocate the PLEvent on the heap.

  nsCOMPtr<nsIEventQueue> eventQ;
  NS_GetMainEventQ(getter_AddRefs(eventQ));
  if (!eventQ)
    return;

  nsGnomeVFSAuthParams params;
  params.in = in;
  params.in_size = in_size;
  params.out = out;
  params.out_size = out_size;
  params.callback_data = callback_data;

  PLEvent ev;
  PL_InitEvent(&ev, &params,
      AuthCallbackEventHandler,
      AuthCallbackEventDestructor);

  void *result;
  if (NS_FAILED(eventQ->PostSynchronousEvent(&ev, &result)))
    PL_DestroyEvent(&ev);
}

//-----------------------------------------------------------------------------

static gint
FileInfoComparator(gconstpointer a, gconstpointer b)
{
  const GnomeVFSFileInfo *ia = (const GnomeVFSFileInfo *) a;
  const GnomeVFSFileInfo *ib = (const GnomeVFSFileInfo *) b;

  return strcasecmp(ia->name, ib->name);
}

//-----------------------------------------------------------------------------

class nsGnomeVFSInputStream : public nsIInputStream
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIINPUTSTREAM

    nsGnomeVFSInputStream(const nsCString &uriSpec)
      : mSpec(uriSpec)
      , mChannel(nsnull)
      , mHandle(nsnull)
      , mBytesRemaining(PR_UINT32_MAX)
      , mStatus(NS_OK)
      , mDirList(nsnull)
      , mDirListPtr(nsnull)
      , mDirBufCursor(0)
      , mDirOpen(PR_FALSE) {}

   ~nsGnomeVFSInputStream() { Close(); }

    void SetChannel(nsIChannel *channel)
    {
      // We need to hold an owning reference to our channel.  This is done
      // so we can access the channel's notification callbacks to acquire
      // a reference to a nsIAuthPrompt if we need to handle a GnomeVFS
      // authentication callback.
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

  private:
    GnomeVFSResult DoOpen();
    GnomeVFSResult DoRead(char *aBuf, PRUint32 aCount, PRUint32 *aCountRead);
    nsresult       SetContentTypeOfChannel(const char *contentType);

  private:
    nsCString                mSpec;
    nsIChannel              *mChannel; // manually refcounted
    GnomeVFSHandle          *mHandle;
    PRUint32                 mBytesRemaining;
    nsresult                 mStatus;
    GList                   *mDirList;
    GList                   *mDirListPtr;
    nsCString                mDirBuf;
    PRUint32                 mDirBufCursor;
    PRPackedBool             mDirOpen;
};

GnomeVFSResult
nsGnomeVFSInputStream::DoOpen()
{
  GnomeVFSResult rv;

  NS_ASSERTION(mHandle == nsnull, "already open");

  // Push a callback handler on the stack for this thread, so we can intercept
  // authentication requests from GnomeVFS.  We'll use the channel to get a
  // nsIAuthPrompt instance.

  gnome_vfs_module_callback_push(GNOME_VFS_MODULE_CALLBACK_AUTHENTICATION,
                                 AuthCallback, mChannel, NULL);

  // Query the mime type first (this could return NULL). 
  //
  // XXX We need to do this up-front in order to determine how to open the URI.
  //     Unfortunately, the error code GNOME_VFS_ERROR_IS_DIRECTORY is not
  //     always returned by gnome_vfs_open when we pass it a URI to a directory!
  //     Otherwise, we could have used that as a way to failover to opening the
  //     URI as a directory.  Also, it would have been ideal if
  //     gnome_vfs_get_file_info_from_handle were actually implemented by the
  //     smb:// module, since that would have allowed us to potentially save a
  //     round trip to the server to discover the mime type of the document in
  //     the case where gnome_vfs_open would have been used.  (Oh well!  /me
  //     throws hands up in the air and moves on...)

  GnomeVFSFileInfo info = {0};
  rv = gnome_vfs_get_file_info(mSpec.get(), &info, GNOME_VFS_FILE_INFO_DEFAULT);
  if (rv == GNOME_VFS_OK && info.type == GNOME_VFS_FILE_TYPE_DIRECTORY)
  {
    rv = gnome_vfs_directory_list_load(&mDirList, mSpec.get(),
                                       GNOME_VFS_FILE_INFO_DEFAULT);

    LOG(("gnomevfs: gnome_vfs_directory_list_load returned %d (%s) [spec=\"%s\"]\n",
        rv, gnome_vfs_result_to_string(rv), mSpec.get()));
  }
  else
  {
    rv = gnome_vfs_open(&mHandle, mSpec.get(), GNOME_VFS_OPEN_READ);

    LOG(("gnomevfs: gnome_vfs_open returned %d (%s) [spec=\"%s\"]\n",
        rv, gnome_vfs_result_to_string(rv), mSpec.get()));
  }

  gnome_vfs_module_callback_pop(GNOME_VFS_MODULE_CALLBACK_AUTHENTICATION);

  if (rv == GNOME_VFS_OK)
  {
    if (mHandle)
    {
      // Here we set the content type of the channel to the value of the mime
      // type determined by GnomeVFS.  However, if GnomeVFS is telling us that
      // the document is binary, we'll ignore that and keep the channel's
      // content type unspecified.  That will enable our content type sniffing
      // algorithms.  This should provide more consistent mime type handling.

      if (info.mime_type && (strcmp(info.mime_type, APPLICATION_OCTET_STREAM) != 0))
        SetContentTypeOfChannel(info.mime_type);

      // XXX truncates size from 64-bit to 32-bit
      mBytesRemaining = (PRUint32) info.size;

      // Update the content length attribute on the channel.  We do this
      // synchronously without proxying.  This hack is not as bad as it looks!
      if (mBytesRemaining != PR_UINT32_MAX)
        mChannel->SetContentLength(mBytesRemaining);
    }
    else
    {
      mDirOpen = PR_TRUE;

      // Sort mDirList
      mDirList = g_list_sort(mDirList, FileInfoComparator);
      mDirListPtr = mDirList;

      // Write base URL (make sure it ends with a '/')
      mDirBuf.Append(NS_LITERAL_CSTRING("300: ") + mSpec);
      if (mSpec.Last() != '/')
        mDirBuf.Append('/');
      mDirBuf.Append('\n');

      // Write column names
      mDirBuf.Append("200: filename content-length last-modified file-type\n");

      // Write charset (assume UTF-8)
      // XXX is this correct?
      mDirBuf.Append("301: UTF-8\n");

      SetContentTypeOfChannel(APPLICATION_HTTP_INDEX_FORMAT);
    }
  }

  gnome_vfs_file_info_clear(&info);
  return rv;
}

GnomeVFSResult
nsGnomeVFSInputStream::DoRead(char *aBuf, PRUint32 aCount, PRUint32 *aCountRead)
{
  GnomeVFSResult rv;

  if (mHandle)
  {
    GnomeVFSFileSize bytesRead;
    rv = gnome_vfs_read(mHandle, aBuf, aCount, &bytesRead);
    if (rv == GNOME_VFS_OK)
    {
      *aCountRead = (PRUint32) bytesRead;
      mBytesRemaining -= *aCountRead;
    }
  }
  else if (mDirOpen)
  {
    rv = GNOME_VFS_OK;

    while (aCount && rv != GNOME_VFS_ERROR_EOF)
    {
      // Copy data out of our buffer
      PRUint32 bufLen = mDirBuf.Length() - mDirBufCursor;
      if (bufLen)
      {
        PRUint32 n = PR_MIN(bufLen, aCount);
        memcpy(aBuf, mDirBuf.get() + mDirBufCursor, n);
        *aCountRead += n;
        aBuf += n;
        aCount -= n;
        mDirBufCursor += n;
      }

      if (!mDirListPtr)    // Are we at the end of the directory list?
      {
        rv = GNOME_VFS_ERROR_EOF;
      }
      else if (aCount)     // Do we need more data?
      {
        GnomeVFSFileInfo *info = (GnomeVFSFileInfo *) mDirListPtr->data;

        // Prune '.' and '..' from directory listing.
        if (info->name[0] == '.' &&
               (info->name[1] == '\0' ||
                   (info->name[1] == '.' && info->name[2] == '\0')))
        {
          mDirListPtr = mDirListPtr->next;
          continue;
        }

        mDirBuf.Assign("201: ");

        // The "filename" field
        char *escName = nsEscape(info->name, url_Path);
        if (escName)
        {
          mDirBuf.Append(escName);
          mDirBuf.Append(' ');
          nsMemory::Free(escName);
        }

        // The "content-length" field
        // XXX truncates size from 64-bit to 32-bit
        mDirBuf.AppendInt(PRInt32(info->size));
        mDirBuf.Append(' ');

        // The "last-modified" field
        // 
        // NSPR promises: PRTime is compatible with time_t
        // we just need to convert from seconds to microseconds
        PRExplodedTime tm;
        PRTime pt = ((PRTime) info->mtime) * 1000000;
        PR_ExplodeTime(pt, PR_GMTParameters, &tm);
        {
          char buf[64];
          PR_FormatTimeUSEnglish(buf, sizeof(buf),
              "%a,%%20%d%%20%b%%20%Y%%20%H:%M:%S%%20GMT ", &tm);
          mDirBuf.Append(buf);
        }

        // The "file-type" field
        switch (info->type)
        {
          case GNOME_VFS_FILE_TYPE_REGULAR:
            mDirBuf.AppendLiteral("FILE ");
            break;
          case GNOME_VFS_FILE_TYPE_DIRECTORY:
            mDirBuf.AppendLiteral("DIRECTORY ");
            break;
          case GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK:
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
  else
  {
    NS_NOTREACHED("reading from what?");
    rv = GNOME_VFS_ERROR_GENERIC;
  }

  return rv;
}

// This class is used to implement SetContentTypeOfChannel.
class nsGnomeVFSSetContentTypeEvent : public PLEvent
{
  public:
    nsGnomeVFSSetContentTypeEvent(nsIChannel *channel, const char *contentType)
      : mContentType(contentType)
    {
      // stash channel reference in owner field.  no AddRef here!  see note
      // in SetContentTypeOfchannel.
      PL_InitEvent(this, channel, EventHandler, EventDestructor);
    }

    PR_STATIC_CALLBACK(void *) EventHandler(PLEvent *ev)
    {
      nsGnomeVFSSetContentTypeEvent *self = (nsGnomeVFSSetContentTypeEvent *) ev;
      ((nsIChannel *) self->owner)->SetContentType(self->mContentType);
      return nsnull;
    }

    PR_STATIC_CALLBACK(void) EventDestructor(PLEvent *ev)
    {
      nsGnomeVFSSetContentTypeEvent *self = (nsGnomeVFSSetContentTypeEvent *) ev;
      delete self;
    }

  private: 
    nsCString mContentType;
};

nsresult
nsGnomeVFSInputStream::SetContentTypeOfChannel(const char *contentType)
{
  // We need to proxy this call over to the main thread.  We post an
  // asynchronous event in this case so that we don't delay reading data, and
  // we know that this is safe to do since the channel's reference will be
  // released asynchronously as well.  We trust the ordering of the main
  // thread's event queue to protect us against memory corruption.

  nsresult rv;
  nsCOMPtr<nsIEventQueue> eventQ;
  rv = NS_GetMainEventQ(getter_AddRefs(eventQ));
  if (NS_FAILED(rv))
    return rv;

  nsGnomeVFSSetContentTypeEvent *ev =
      new nsGnomeVFSSetContentTypeEvent(mChannel, contentType);
  if (!ev)
  {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  else
  {
    rv = eventQ->PostEvent(ev);
    if (NS_FAILED(rv))
      PL_DestroyEvent(ev);
  }
  return rv;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsGnomeVFSInputStream, nsIInputStream)

NS_IMETHODIMP
nsGnomeVFSInputStream::Close()
{
  if (mHandle)
  {
    gnome_vfs_close(mHandle);
    mHandle = nsnull;
  }

  if (mDirList)
  {
    // Destroy the list of GnomeVFSFileInfo objects...
    g_list_foreach(mDirList, (GFunc) gnome_vfs_file_info_unref, nsnull);
    g_list_free(mDirList);
    mDirList = nsnull;
    mDirListPtr = nsnull;
  }

  if (mChannel)
  {
    nsresult rv;

    nsCOMPtr<nsIEventQueue> eventQ;
    rv = NS_GetMainEventQ(getter_AddRefs(eventQ));
    if (NS_SUCCEEDED(rv))
      rv = NS_ProxyRelease(eventQ, mChannel);

    NS_ASSERTION(NS_SUCCEEDED(rv), "leaking channel reference");
    mChannel = nsnull;
  }

  mSpec.Truncate(); // free memory

  // Prevent future reads from re-opening the handle.
  if (NS_SUCCEEDED(mStatus))
    mStatus = NS_BASE_STREAM_CLOSED;

  return NS_OK;
}

NS_IMETHODIMP
nsGnomeVFSInputStream::Available(PRUint32 *aResult)
{
  if (NS_FAILED(mStatus))
    return mStatus;

  *aResult = mBytesRemaining;
  return NS_OK;
}

NS_IMETHODIMP
nsGnomeVFSInputStream::Read(char *aBuf,
                            PRUint32 aCount,
                            PRUint32 *aCountRead)
{
  *aCountRead = 0;

  if (NS_FAILED(mStatus))
    return mStatus;

  GnomeVFSResult rv = GNOME_VFS_OK;

  // If this is our first-time through here, then open the URI.
  if (!mHandle && !mDirOpen)
    rv = DoOpen();
  
  if (rv == GNOME_VFS_OK)
    rv = DoRead(aBuf, aCount, aCountRead);

  if (rv != GNOME_VFS_OK)
  {
    // If we reach here, we hit some kind of error.  EOF is not an error.
    mStatus = MapGnomeVFSResult(rv);
    if (mStatus == NS_BASE_STREAM_CLOSED)
      return NS_OK;

    LOG(("gnomevfs: result %d [%s] mapped to 0x%x\n",
        rv, gnome_vfs_result_to_string(rv), mStatus));
  }
  return mStatus;
}

NS_IMETHODIMP
nsGnomeVFSInputStream::ReadSegments(nsWriteSegmentFun aWriter,
                                    void *aClosure,
                                    PRUint32 aCount,
                                    PRUint32 *aResult)
{
  // There is no way to implement this using GnomeVFS, but fortunately
  // that doesn't matter.  Because we are a blocking input stream, Necko
  // isn't going to call our ReadSegments method.
  NS_NOTREACHED("nsGnomeVFSInputStream::ReadSegments");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsGnomeVFSInputStream::IsNonBlocking(PRBool *aResult)
{
  *aResult = PR_FALSE;
  return NS_OK;
}

//-----------------------------------------------------------------------------

class nsGnomeVFSProtocolHandler : public nsIProtocolHandler
                                , public nsIObserver
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROTOCOLHANDLER
    NS_DECL_NSIOBSERVER

    nsresult Init();

  private:
    void   InitSupportedProtocolsPref(nsIPrefBranch *prefs);
    PRBool IsSupportedProtocol(const nsCString &spec);

    nsXPIDLCString mSupportedProtocols;
};

NS_IMPL_ISUPPORTS2(nsGnomeVFSProtocolHandler, nsIProtocolHandler, nsIObserver)

nsresult
nsGnomeVFSProtocolHandler::Init()
{
#ifdef PR_LOGGING
  sGnomeVFSLog = PR_NewLogModule("gnomevfs");
#endif

  if (!gnome_vfs_initialized())
  {
    if (!gnome_vfs_init())
    {
      NS_WARNING("gnome_vfs_init failed");
      return NS_ERROR_UNEXPECTED;
    }
  }

  nsCOMPtr<nsIPrefBranchInternal> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs)
  {
    InitSupportedProtocolsPref(prefs);
    prefs->AddObserver(MOZ_GNOMEVFS_SUPPORTED_PROTOCOLS, this, PR_FALSE);
  }

  return NS_OK;
}

void
nsGnomeVFSProtocolHandler::InitSupportedProtocolsPref(nsIPrefBranch *prefs)
{
  // read preferences
  nsresult rv = prefs->GetCharPref(MOZ_GNOMEVFS_SUPPORTED_PROTOCOLS,
                                   getter_Copies(mSupportedProtocols));
  if (NS_SUCCEEDED(rv))
    mSupportedProtocols.StripWhitespace();
  else
    mSupportedProtocols.AssignLiteral("smb:,sftp:"); // use defaults

  LOG(("gnomevfs: supported protocols \"%s\"\n", mSupportedProtocols.get()));
}

PRBool
nsGnomeVFSProtocolHandler::IsSupportedProtocol(const nsCString &spec)
{
  PRInt32 colon = spec.FindChar(':');
  if (colon == kNotFound)
    return PR_FALSE;

  // <scheme> + ':'
  const nsCSubstring &scheme = StringHead(spec, colon + 1);

  nsACString::const_iterator begin, end, iter;
  mSupportedProtocols.BeginReading(begin);
  mSupportedProtocols.EndReading(end);

  iter = begin;
  return CaseInsensitiveFindInReadable(scheme, iter, end) &&
      (iter == begin || *(--iter) == ',');
}

NS_IMETHODIMP
nsGnomeVFSProtocolHandler::GetScheme(nsACString &aScheme)
{
  aScheme.AssignLiteral(MOZ_GNOMEVFS_SCHEME);
  return NS_OK;
}

NS_IMETHODIMP
nsGnomeVFSProtocolHandler::GetDefaultPort(PRInt32 *aDefaultPort)
{
  *aDefaultPort = -1;
  return NS_OK;
}

NS_IMETHODIMP
nsGnomeVFSProtocolHandler::GetProtocolFlags(PRUint32 *aProtocolFlags)
{
  // Is this true of all GnomeVFS URI types?
  *aProtocolFlags = URI_STD;
  return NS_OK;
}

NS_IMETHODIMP
nsGnomeVFSProtocolHandler::NewURI(const nsACString &aSpec,
                                  const char *aOriginCharset,
                                  nsIURI *aBaseURI,
                                  nsIURI **aResult)
{
  const nsPromiseFlatCString &flatSpec = PromiseFlatCString(aSpec);

  LOG(("gnomevfs: NewURI [spec=%s]\n", flatSpec.get()));

  if (!aBaseURI)
  {
    //
    // XXX This check is used to limit the gnome-vfs protocols we support.  For
    //     security reasons, it is best that we limit the protocols we support to
    //     those with known characteristics.  We might want to lessen this
    //     restriction if it proves to be too heavy handed.  A black list of
    //     protocols we don't want to support might be better.  For example, we
    //     probably don't want to try to load "start-here:" inside the browser.
    //     There are others that fall into this category, which are best handled
    //     externally by Nautilus (or another app like it).
    //
    if (!IsSupportedProtocol(flatSpec))
      return NS_ERROR_UNKNOWN_PROTOCOL;

    // Verify that GnomeVFS supports this URI scheme.
    GnomeVFSURI *uri = gnome_vfs_uri_new(flatSpec.get());
    if (!uri)
      return NS_ERROR_UNKNOWN_PROTOCOL;
  }

  //
  // XXX Can we really assume that all gnome-vfs URIs can be parsed using
  //     nsStandardURL?  We probably really need to implement nsIURI/nsIURL
  //     in terms of the gnome_vfs_uri_XXX methods, but at least this works
  //     correctly for smb:// URLs ;-)
  //
  //     Also, it might not be possible to fully implement nsIURI/nsIURL in
  //     terms of GnomeVFSURI since some Necko methods have no GnomeVFS
  //     equivalent.
  //
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
nsGnomeVFSProtocolHandler::NewChannel(nsIURI *aURI, nsIChannel **aResult)
{
  nsresult rv;

  nsCAutoString spec;
  rv = aURI->GetSpec(spec);
  if (NS_FAILED(rv))
    return rv;

  nsRefPtr<nsGnomeVFSInputStream> stream = new nsGnomeVFSInputStream(spec);
  if (!stream)
  {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  else
  {
    // start out assuming an unknown content-type.  we'll set the content-type
    // to something better once we open the URI.
    rv = NS_NewInputStreamChannel(aResult, aURI, stream,
                                  NS_LITERAL_CSTRING(UNKNOWN_CONTENT_TYPE));
    if (NS_SUCCEEDED(rv))
      stream->SetChannel(*aResult);
  }
  return rv;
}

NS_IMETHODIMP
nsGnomeVFSProtocolHandler::AllowPort(PRInt32 aPort,
                                     const char *aScheme,
                                     PRBool *aResult)
{
  // Don't override anything.
  *aResult = PR_FALSE; 
  return NS_OK;
}

NS_IMETHODIMP
nsGnomeVFSProtocolHandler::Observe(nsISupports *aSubject,
                                   const char *aTopic,
                                   const PRUnichar *aData)
{
  if (strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID) == 0) {
    nsCOMPtr<nsIPrefBranch> prefs = do_QueryInterface(aSubject);
    InitSupportedProtocolsPref(prefs);
  }
  return NS_OK;
}

//-----------------------------------------------------------------------------

#define NS_GNOMEVFSPROTOCOLHANDLER_CID               \
{ /* 9b6dc177-a2e4-49e1-9c98-0a8384de7f6c */         \
    0x9b6dc177,                                      \
    0xa2e4,                                          \
    0x49e1,                                          \
    {0x9c, 0x98, 0x0a, 0x83, 0x84, 0xde, 0x7f, 0x6c} \
}

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsGnomeVFSProtocolHandler, Init)

static const nsModuleComponentInfo components[] =
{
  { "nsGnomeVFSProtocolHandler",
    NS_GNOMEVFSPROTOCOLHANDLER_CID,
    NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX MOZ_GNOMEVFS_SCHEME,
    nsGnomeVFSProtocolHandlerConstructor
  }
};

NS_IMPL_NSGETMODULE(nsGnomeVFSModule, components)
