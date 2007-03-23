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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Christopher Blizzard. Portions created by Christopher Blizzard are Copyright (C) Christopher Blizzard.  All Rights Reserved.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
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
/**
 * Derived from GContentHandler http://landfill.mozilla.org/mxr-test/gnome/source/galeon/mozilla/ContentHandler.h
 */
#ifndef __EmbedDownloadMgr_h
#define __EmbedDownloadMgr_h

#include "EmbedPrivate.h"
#include "nsIHelperAppLauncherDialog.h"
#include "nsIMIMEInfo.h"
#include "nsCOMPtr.h"
#include "nsIExternalHelperAppService.h"
#include "nsIRequest.h"
#include "nsILocalFile.h"

#include "nsWeakReference.h"
#define EMBED_DOWNLOADMGR_DESCRIPTION "MicroB Download Manager"
#define EMBED_DOWNLOADMGR_CID {0x53df12a2, 0x1f4a, 0x4382, {0x99, 0x4e, 0xed, 0x62, 0xcf, 0x0d, 0x6b, 0x3a}}

class nsIURI;
class nsIFile;
class nsIFactory;
class nsExternalAppHandler;

typedef struct _EmbedDownload EmbedDownload;

struct _EmbedDownload
{
  GtkObject*  parent;
  GtkWidget*  gtkMozEmbedParentWidget;/** Associated gtkmozembed widget */

  char*       file_name;             /** < The file's name */
  char*       file_name_with_path;   /** < The file's name */
  const char* server;                /** < The server's name */
  const char* file_type;             /** < The file's type */
  const char* handler_app;           /** < The application's name */
  PRInt64     file_size;             /** < The file's size */
  PRInt64     downloaded_size;       /** < The download's size */
  gboolean    is_paused;             /** < If download is paused or not */
  gboolean    open_with;             /** < If the file can be opened by other application */

  /* Pointer to mozilla interfaces */
  nsIHelperAppLauncher* launcher;    /** < The mozilla's download dialog */
  nsIRequest* request;               /** < The download request */
};

class EmbedDownloadMgr : public nsIHelperAppLauncherDialog
{
  public:
    EmbedDownloadMgr();
    virtual ~EmbedDownloadMgr();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIHELPERAPPLAUNCHERDIALOG

  private:
    /** Gets all informations about a file which is being downloaded.
    */
    NS_METHOD GetDownloadInfo(nsIHelperAppLauncher *aLauncher, nsISupports *aContext);

    EmbedDownload *mDownload;
};
#endif /* __EmbedDownloadMgr_h */
