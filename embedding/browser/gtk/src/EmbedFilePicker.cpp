/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 tw=80 et cindent: */
/*
 * ***** BEGIN LICENSE BLOCK *****
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
 * timeless <timeless@mozdev.org>.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Antonio Gomes <tonikitoo@gmail.com>
 *   Oleg Romashin <romaxa@gmail.com>
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

#include "EmbedFilePicker.h"
#include "EmbedGtkTools.h"
#include "nsIFileURL.h"
#include "nsILocalFile.h"
#include "nsIDOMWindow.h"
#include "nsStringGlue.h"
#include "nsNetUtil.h"
#include "prenv.h"

#ifdef MOZ_LOGGING
#include <stdlib.h>
#endif

NS_IMPL_ISUPPORTS1(EmbedFilePicker, nsIFilePicker)

EmbedFilePicker::EmbedFilePicker(): mParent (nsnull),
                                    mMode(nsIFilePicker::modeOpen)
{
}

EmbedFilePicker::~EmbedFilePicker()
{
}

/* void init (in nsIDOMWindowInternal parent, in wstring title, in short mode); */
NS_IMETHODIMP EmbedFilePicker::Init(nsIDOMWindow *parent, const nsAString &title, PRInt16 mode)
{
  mParent = parent;
  mMode = mode;
  return NS_OK;
}

/* void appendFilters (in long filterMask); */
NS_IMETHODIMP EmbedFilePicker::AppendFilters(PRInt32 filterMask)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void appendFilter (in wstring title, in wstring filter); */
NS_IMETHODIMP EmbedFilePicker::AppendFilter(const nsAString &title, const nsAString &filter)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute wstring defaultString; */
NS_IMETHODIMP EmbedFilePicker::GetDefaultString(nsAString &aDefaultString)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP EmbedFilePicker::SetDefaultString(const nsAString &aDefaultString)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute wstring defaultExtension; */
NS_IMETHODIMP EmbedFilePicker::GetDefaultExtension(nsAString &aDefaultExtension)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP EmbedFilePicker::SetDefaultExtension(const nsAString &aDefaultExtension)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute long filterIndex; */
NS_IMETHODIMP EmbedFilePicker::GetFilterIndex(PRInt32 *aFilterIndex)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP EmbedFilePicker::SetFilterIndex(PRInt32 aFilterIndex)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute nsILocalFile displayDirectory; */
NS_IMETHODIMP EmbedFilePicker::GetDisplayDirectory(nsILocalFile **aDisplayDirectory)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP EmbedFilePicker::SetDisplayDirectory(nsILocalFile *aDisplayDirectory)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsILocalFile file; */
NS_IMETHODIMP EmbedFilePicker::GetFile(nsILocalFile **aFile)
{

  NS_ENSURE_ARG_POINTER(aFile);

  if (mFilename.IsEmpty())
    return NS_OK;

  nsCOMPtr<nsIURI> baseURI;
  nsresult rv = NS_NewURI(getter_AddRefs(baseURI), mFilename);

  nsCOMPtr<nsIFileURL> fileURL(do_QueryInterface(baseURI, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFile> file;
  rv = fileURL->GetFile(getter_AddRefs(file));

  nsCOMPtr<nsILocalFile> localfile;
  localfile = do_QueryInterface(file, &rv);

  if (NS_SUCCEEDED(rv)) {
    NS_ADDREF(*aFile = localfile);
    return NS_OK;
  }

  NS_ENSURE_TRUE(mParent, NS_OK);
  GtkWidget* parentWidget = GetGtkWidgetForDOMWindow(mParent);
  NS_ENSURE_TRUE(parentWidget, NS_OK);
  /* XXX this message isn't really sure what it's trying to say */
  g_signal_emit_by_name(GTK_OBJECT(parentWidget), "alert", "File protocol not supported.", NULL);
  return NS_OK;
}

/* readonly attribute nsIFileURL fileURL; */
NS_IMETHODIMP EmbedFilePicker::GetFileURL(nsIFileURL **aFileURL)
{
  NS_ENSURE_ARG_POINTER(aFileURL);
  *aFileURL = nsnull;

  nsCOMPtr<nsILocalFile> file;
  GetFile(getter_AddRefs(file));
  NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);
  nsCOMPtr<nsIFileURL> fileURL = do_CreateInstance(NS_STANDARDURL_CONTRACTID);
  NS_ENSURE_TRUE(fileURL, NS_ERROR_OUT_OF_MEMORY);
  fileURL->SetFile(file);
  NS_ADDREF(*aFileURL = fileURL);
  return NS_OK;
}

/* readonly attribute nsISimpleEnumerator files; */
NS_IMETHODIMP EmbedFilePicker::GetFiles(nsISimpleEnumerator * *aFiles)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* short show (); */
NS_IMETHODIMP EmbedFilePicker::Show(PRInt16 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  NS_ENSURE_TRUE(mParent, NS_OK);

  GtkWidget *parentWidget = GetGtkWidgetForDOMWindow(mParent);
  NS_ENSURE_TRUE(parentWidget, NS_OK);

  gboolean response = 0;
  char *retname = nsnull;
  g_signal_emit_by_name(GTK_OBJECT(parentWidget),
                        "upload_dialog",
                        PR_GetEnv("HOME"),
                        "",
                        &retname,
                        &response,
                        NULL);

  *_retval = response ? nsIFilePicker::returnOK : nsIFilePicker::returnCancel;

  mFilename = retname;
  if (retname)
    NS_Free(retname);

  return NS_OK;
}
