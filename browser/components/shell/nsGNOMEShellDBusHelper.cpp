/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGNOMEShellSearchProvider.h"

#include "RemoteUtils.h"
#include "nsIStringBundle.h"
#include "nsServiceManagerUtils.h"
#include "nsPrintfCString.h"
#include "mozilla/XREAppData.h"
#include "nsAppRunner.h"

#define DBUS_BUS_NAME_TEMPLATE "org.mozilla.%s.SearchProvider"
#define DBUS_OBJECT_PATH_TEMPLATE "/org/mozilla/%s/SearchProvider"

const char* GetDBusBusName() {
  static const char* name = []() {
    nsAutoCString appName;
    gAppData->GetDBusAppName(appName);
    return ToNewCString(nsPrintfCString(DBUS_BUS_NAME_TEMPLATE,
                                        appName.get()));  // Intentionally leak
  }();
  return name;
}

const char* GetDBusObjectPath() {
  static const char* path = []() {
    nsAutoCString appName;
    gAppData->GetDBusAppName(appName);
    return ToNewCString(nsPrintfCString(DBUS_OBJECT_PATH_TEMPLATE,
                                        appName.get()));  // Intentionally leak
  }();
  return path;
}

static bool GetGnomeSearchTitle(const char* aSearchedTerm,
                                nsAutoCString& aGnomeSearchTitle) {
  static nsCOMPtr<nsIStringBundle> bundle;
  if (!bundle) {
    nsCOMPtr<nsIStringBundleService> sbs =
        do_GetService(NS_STRINGBUNDLE_CONTRACTID);
    if (NS_WARN_IF(!sbs)) {
      return false;
    }

    sbs->CreateBundle("chrome://browser/locale/browser.properties",
                      getter_AddRefs(bundle));
    if (NS_WARN_IF(!bundle)) {
      return false;
    }
  }

  AutoTArray<nsString, 1> formatStrings;
  CopyUTF8toUTF16(nsCString(aSearchedTerm), *formatStrings.AppendElement());

  nsAutoString gnomeSearchTitle;
  bundle->FormatStringFromName("gnomeSearchProviderSearchWeb", formatStrings,
                               gnomeSearchTitle);
  AppendUTF16toUTF8(gnomeSearchTitle, aGnomeSearchTitle);
  return true;
}

static const char* introspect_template =
    "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection "
    "1.0//EN\"\n"
    "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
    "<node>\n"
    " <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
    "   <method name=\"Introspect\">\n"
    "     <arg name=\"data\" direction=\"out\" type=\"s\"/>\n"
    "   </method>\n"
    " </interface>\n"
    " <interface name=\"org.gnome.Shell.SearchProvider2\">\n"
    "   <method name=\"GetInitialResultSet\">\n"
    "     <arg type=\"as\" name=\"terms\" direction=\"in\" />\n"
    "     <arg type=\"as\" name=\"results\" direction=\"out\" />\n"
    "   </method>\n"
    "   <method name=\"GetSubsearchResultSet\">\n"
    "     <arg type=\"as\" name=\"previous_results\" direction=\"in\" />\n"
    "     <arg type=\"as\" name=\"terms\" direction=\"in\" />\n"
    "     <arg type=\"as\" name=\"results\" direction=\"out\" />\n"
    "   </method>\n"
    "   <method name=\"GetResultMetas\">\n"
    "     <arg type=\"as\" name=\"identifiers\" direction=\"in\" />\n"
    "     <arg type=\"aa{sv}\" name=\"metas\" direction=\"out\" />\n"
    "   </method>\n"
    "   <method name=\"ActivateResult\">\n"
    "     <arg type=\"s\" name=\"identifier\" direction=\"in\" />\n"
    "     <arg type=\"as\" name=\"terms\" direction=\"in\" />\n"
    "     <arg type=\"u\" name=\"timestamp\" direction=\"in\" />\n"
    "   </method>\n"
    "   <method name=\"LaunchSearch\">\n"
    "     <arg type=\"as\" name=\"terms\" direction=\"in\" />\n"
    "     <arg type=\"u\" name=\"timestamp\" direction=\"in\" />\n"
    "   </method>\n"
    "</interface>\n"
    "</node>\n";

DBusHandlerResult DBusIntrospect(DBusConnection* aConnection,
                                 DBusMessage* aMsg) {
  DBusMessage* reply;

  reply = dbus_message_new_method_return(aMsg);
  if (!reply) {
    return DBUS_HANDLER_RESULT_NEED_MEMORY;
  }

  dbus_message_append_args(reply, DBUS_TYPE_STRING, &introspect_template,
                           DBUS_TYPE_INVALID);

  dbus_connection_send(aConnection, reply, nullptr);
  dbus_message_unref(reply);

  return DBUS_HANDLER_RESULT_HANDLED;
}

int DBusGetIndexFromIDKey(const char* aIDKey) {
  // ID is NN:URL where NN is index to our current history
  // result container.
  char tmp[] = {aIDKey[0], aIDKey[1], '\0'};
  return atoi(tmp);
}

static void ConcatArray(nsACString& aOutputStr, const char** aStringArray) {
  for (const char** term = aStringArray; *term; term++) {
    aOutputStr.Append(*term);
    if (*(term + 1)) {
      aOutputStr.Append(" ");
    }
  }
}

DBusHandlerResult DBusHandleInitialResultSet(
    RefPtr<nsGNOMEShellHistorySearchResult> aSearchResult, DBusMessage* aMsg) {
  DBusMessage* reply;
  char** stringArray = nullptr;
  int elements;

  if (!dbus_message_get_args(aMsg, nullptr, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING,
                             &stringArray, &elements, DBUS_TYPE_INVALID) ||
      elements == 0) {
    reply = dbus_message_new_error(aMsg, GetDBusBusName(), "Wrong argument");
    dbus_connection_send(aSearchResult->GetDBusConnection(), reply, nullptr);
    dbus_message_unref(reply);
  } else {
    aSearchResult->SetReply(dbus_message_new_method_return(aMsg));
    nsAutoCString searchTerm;
    ConcatArray(searchTerm, const_cast<const char**>(stringArray));
    aSearchResult->SetSearchTerm(searchTerm.get());
    GetGNOMEShellHistoryService()->QueryHistory(aSearchResult);
    // DBus reply will be send asynchronously by
    // nsGNOMEShellHistorySearchResult::SendDBusSearchResultReply()
    // when GetGNOMEShellHistoryService() has the results.
  }

  if (stringArray) {
    dbus_free_string_array(stringArray);
  }

  return DBUS_HANDLER_RESULT_HANDLED;
}

DBusHandlerResult DBusHandleSubsearchResultSet(
    RefPtr<nsGNOMEShellHistorySearchResult> aSearchResult, DBusMessage* aMsg) {
  DBusMessage* reply;

  char **unusedArray = nullptr, **stringArray = nullptr;
  int unusedNum, elements;

  if (!dbus_message_get_args(aMsg, nullptr, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING,
                             &unusedArray, &unusedNum, DBUS_TYPE_ARRAY,
                             DBUS_TYPE_STRING, &stringArray, &elements,
                             DBUS_TYPE_INVALID) ||
      elements == 0) {
    reply = dbus_message_new_error(aMsg, GetDBusBusName(), "Wrong argument");
    dbus_connection_send(aSearchResult->GetDBusConnection(), reply, nullptr);
    dbus_message_unref(reply);
  } else {
    aSearchResult->SetReply(dbus_message_new_method_return(aMsg));
    nsAutoCString searchTerm;
    ConcatArray(searchTerm, const_cast<const char**>(stringArray));
    aSearchResult->SetSearchTerm(searchTerm.get());
    GetGNOMEShellHistoryService()->QueryHistory(aSearchResult);
    // DBus reply will be send asynchronously by
    // nsGNOMEShellHistorySearchResult::SendDBusSearchResultReply()
    // when GetGNOMEShellHistoryService() has the results.
  }

  if (unusedArray) {
    dbus_free_string_array(unusedArray);
  }
  if (stringArray) {
    dbus_free_string_array(stringArray);
  }

  return DBUS_HANDLER_RESULT_HANDLED;
}

static void appendStringDictionary(DBusMessageIter* aIter, const char* aKey,
                                   const char* aValue) {
  DBusMessageIter iterDict, iterVar;
  dbus_message_iter_open_container(aIter, DBUS_TYPE_DICT_ENTRY, nullptr,
                                   &iterDict);
  dbus_message_iter_append_basic(&iterDict, DBUS_TYPE_STRING, &aKey);
  dbus_message_iter_open_container(&iterDict, DBUS_TYPE_VARIANT, "s", &iterVar);
  dbus_message_iter_append_basic(&iterVar, DBUS_TYPE_STRING, &aValue);
  dbus_message_iter_close_container(&iterDict, &iterVar);
  dbus_message_iter_close_container(aIter, &iterDict);
}

/*
  "icon-data": a tuple of type (iiibiiay) describing a pixbuf with width,
              height, rowstride, has-alpha,
              bits-per-sample, channels,
              image data
*/
static void DBusAppendIcon(GnomeHistoryIcon* aIcon, DBusMessageIter* aIter) {
  DBusMessageIter iterDict, iterVar, iterStruct;
  dbus_message_iter_open_container(aIter, DBUS_TYPE_DICT_ENTRY, nullptr,
                                   &iterDict);
  const char* key = "icon-data";
  dbus_message_iter_append_basic(&iterDict, DBUS_TYPE_STRING, &key);
  dbus_message_iter_open_container(&iterDict, DBUS_TYPE_VARIANT, "(iiibiiay)",
                                   &iterVar);
  dbus_message_iter_open_container(&iterVar, DBUS_TYPE_STRUCT, nullptr,
                                   &iterStruct);

  int width = aIcon->GetWidth();
  int height = aIcon->GetHeight();
  dbus_message_iter_append_basic(&iterStruct, DBUS_TYPE_INT32, &width);
  dbus_message_iter_append_basic(&iterStruct, DBUS_TYPE_INT32, &height);
  int rowstride = width * 4;
  dbus_message_iter_append_basic(&iterStruct, DBUS_TYPE_INT32, &rowstride);
  int hasAlpha = true;
  dbus_message_iter_append_basic(&iterStruct, DBUS_TYPE_BOOLEAN, &hasAlpha);
  int bitsPerSample = 8;
  dbus_message_iter_append_basic(&iterStruct, DBUS_TYPE_INT32, &bitsPerSample);
  int channels = 4;
  dbus_message_iter_append_basic(&iterStruct, DBUS_TYPE_INT32, &channels);

  DBusMessageIter iterArray;
  dbus_message_iter_open_container(&iterStruct, DBUS_TYPE_ARRAY, "y",
                                   &iterArray);
  unsigned char* array = aIcon->GetData();
  dbus_message_iter_append_fixed_array(&iterArray, DBUS_TYPE_BYTE, &array,
                                       width * height * 4);
  dbus_message_iter_close_container(&iterStruct, &iterArray);

  dbus_message_iter_close_container(&iterVar, &iterStruct);
  dbus_message_iter_close_container(&iterDict, &iterVar);
  dbus_message_iter_close_container(aIter, &iterDict);
}

/* Appends history search results to the DBUS reply.

  We can return those fields at GetResultMetas:

  "id": the result ID
  "name": the display name for the result
  "icon": a serialized GIcon (see g_icon_serialize()), or alternatively,
  "gicon": a textual representation of a GIcon (see g_icon_to_string()),
           or alternativly,
  "icon-data": a tuple of type (iiibiiay) describing a pixbuf with width,
              height, rowstride, has-alpha, bits-per-sample, and image data
  "description": an optional short description (1-2 lines)
*/
static void DBusAppendResultID(
    RefPtr<nsGNOMEShellHistorySearchResult> aSearchResult,
    DBusMessageIter* aIter, const char* aID) {
  nsCOMPtr<nsINavHistoryContainerResultNode> container =
      aSearchResult->GetSearchResultContainer();

  int index = DBusGetIndexFromIDKey(aID);
  nsCOMPtr<nsINavHistoryResultNode> child;
  container->GetChild(index, getter_AddRefs(child));
  nsAutoCString title;
  if (NS_FAILED(child->GetTitle(title))) {
    return;
  }

  if (title.IsEmpty()) {
    if (NS_FAILED(child->GetUri(title)) || title.IsEmpty()) {
      return;
    }
  }

  const char* titleStr = title.get();
  appendStringDictionary(aIter, "id", aID);
  appendStringDictionary(aIter, "name", titleStr);

  GnomeHistoryIcon* icon = aSearchResult->GetHistoryIcon(index);
  if (icon) {
    DBusAppendIcon(icon, aIter);
  } else {
    appendStringDictionary(aIter, "gicon", "text-html");
  }
}

/* Search the web for: "searchTerm" to the DBUS reply.
 */
static void DBusAppendSearchID(DBusMessageIter* aIter, const char* aID) {
  /* aID contains:

     KEYWORD_SEARCH_STRING:ssssss

     KEYWORD_SEARCH_STRING is a 'special:search' keyword
     ssssss is a searched term, must be at least one character long
  */

  // aID contains only 'KEYWORD_SEARCH_STRING:' so we're missing searched
  // string.
  if (strlen(aID) <= KEYWORD_SEARCH_STRING_LEN + 1) {
    return;
  }

  appendStringDictionary(aIter, "id", KEYWORD_SEARCH_STRING);

  // Extract ssssss part from aID
  auto searchTerm = nsAutoCStringN<32>(aID + KEYWORD_SEARCH_STRING_LEN + 1);
  nsAutoCString gnomeSearchTitle;
  if (GetGnomeSearchTitle(searchTerm.get(), gnomeSearchTitle)) {
    appendStringDictionary(aIter, "name", gnomeSearchTitle.get());
    // TODO: When running on flatpak/snap we may need to use
    // icon like org.mozilla.Firefox or so.
    appendStringDictionary(aIter, "gicon", "firefox");
  }
}

DBusHandlerResult DBusHandleResultMetas(
    RefPtr<nsGNOMEShellHistorySearchResult> aSearchResult, DBusMessage* aMsg) {
  DBusMessage* reply;
  char** stringArray;
  int elements;

  if (!dbus_message_get_args(aMsg, nullptr, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING,
                             &stringArray, &elements, DBUS_TYPE_INVALID) ||
      elements == 0) {
    reply = dbus_message_new_error(aMsg, GetDBusBusName(), "Wrong argument");
  } else {
    reply = dbus_message_new_method_return(aMsg);

    DBusMessageIter iter;
    dbus_message_iter_init_append(reply, &iter);
    DBusMessageIter iterArray;
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "a{sv}",
                                     &iterArray);

    DBusMessageIter iterArray2;
    for (int i = 0; i < elements; i++) {
      dbus_message_iter_open_container(&iterArray, DBUS_TYPE_ARRAY, "{sv}",
                                       &iterArray2);
      if (strncmp(stringArray[i], KEYWORD_SEARCH_STRING,
                  KEYWORD_SEARCH_STRING_LEN) == 0) {
        DBusAppendSearchID(&iterArray2, stringArray[i]);
      } else {
        DBusAppendResultID(aSearchResult, &iterArray2, stringArray[i]);
      }
      dbus_message_iter_close_container(&iterArray, &iterArray2);
    }

    dbus_message_iter_close_container(&iter, &iterArray);
    dbus_free_string_array(stringArray);
  }

  dbus_connection_send(aSearchResult->GetDBusConnection(), reply, nullptr);
  dbus_message_unref(reply);

  return DBUS_HANDLER_RESULT_HANDLED;
}  // namespace mozilla

static void ActivateResultID(
    RefPtr<nsGNOMEShellHistorySearchResult> aSearchResult,
    const char* aResultID, uint32_t aTimeStamp) {
  char* commandLine = nullptr;
  int tmp;

  if (strncmp(aResultID, KEYWORD_SEARCH_STRING, KEYWORD_SEARCH_STRING_LEN) ==
      0) {
    const char* urlList[3] = {"unused", "--search",
                              aSearchResult->GetSearchTerm().get()};
    commandLine = ConstructCommandLine(std::size(urlList), (char**)urlList,
                                       nullptr, &tmp);
  } else {
    int keyIndex = atoi(aResultID);
    nsCOMPtr<nsINavHistoryResultNode> child;
    aSearchResult->GetSearchResultContainer()->GetChild(keyIndex,
                                                        getter_AddRefs(child));
    if (!child) {
      return;
    }

    nsAutoCString uri;
    nsresult rv = child->GetUri(uri);
    if (NS_FAILED(rv)) {
      return;
    }

    const char* urlList[2] = {"unused", uri.get()};
    commandLine = ConstructCommandLine(std::size(urlList), (char**)urlList,
                                       nullptr, &tmp);
  }

  if (commandLine) {
    aSearchResult->HandleCommandLine(commandLine, aTimeStamp);
    free(commandLine);
  }
}

static void DBusLaunchWithAllResults(
    RefPtr<nsGNOMEShellHistorySearchResult> aSearchResult,
    uint32_t aTimeStamp) {
  uint32_t childCount = 0;
  nsresult rv =
      aSearchResult->GetSearchResultContainer()->GetChildCount(&childCount);
  if (NS_FAILED(rv) || childCount == 0) {
    return;
  }

  if (childCount > MAX_SEARCH_RESULTS_NUM) {
    childCount = MAX_SEARCH_RESULTS_NUM;
  }

  // Allocate space for all found results, "unused", "--search" and
  // potential search request.
  char** urlList = (char**)moz_xmalloc(sizeof(char*) * (childCount + 3));
  int urlListElements = 0;

  urlList[urlListElements++] = strdup("unused");

  for (uint32_t i = 0; i < childCount; i++) {
    nsCOMPtr<nsINavHistoryResultNode> child;
    aSearchResult->GetSearchResultContainer()->GetChild(i,
                                                        getter_AddRefs(child));

    if (!IsHistoryResultNodeURI(child)) {
      continue;
    }

    nsAutoCString uri;
    nsresult rv = child->GetUri(uri);
    if (NS_FAILED(rv)) {
      continue;
    }
    urlList[urlListElements++] = strdup(uri.get());
  }

  // When there isn't any uri to open pass search at least.
  if (!childCount) {
    urlList[urlListElements++] = strdup("--search");
    urlList[urlListElements++] = strdup(aSearchResult->GetSearchTerm().get());
  }

  int tmp;
  char* commandLine =
      ConstructCommandLine(urlListElements, urlList, nullptr, &tmp);
  if (commandLine) {
    aSearchResult->HandleCommandLine(commandLine, aTimeStamp);
    free(commandLine);
  }

  for (int i = 0; i < urlListElements; i++) {
    free(urlList[i]);
  }
  free(urlList);
}

DBusHandlerResult DBusActivateResult(
    RefPtr<nsGNOMEShellHistorySearchResult> aSearchResult, DBusMessage* aMsg) {
  DBusMessage* reply;
  char* resultID;
  char** stringArray;
  int elements;
  uint32_t timestamp;

  if (!dbus_message_get_args(aMsg, nullptr, DBUS_TYPE_STRING, &resultID,
                             DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &stringArray,
                             &elements, DBUS_TYPE_UINT32, &timestamp,
                             DBUS_TYPE_INVALID) ||
      resultID == nullptr) {
    reply = dbus_message_new_error(aMsg, GetDBusBusName(), "Wrong argument");
  } else {
    reply = dbus_message_new_method_return(aMsg);
    ActivateResultID(aSearchResult, resultID, timestamp);
    dbus_free_string_array(stringArray);
  }

  dbus_connection_send(aSearchResult->GetDBusConnection(), reply, nullptr);
  dbus_message_unref(reply);

  return DBUS_HANDLER_RESULT_HANDLED;
}

DBusHandlerResult DBusLaunchSearch(
    RefPtr<nsGNOMEShellHistorySearchResult> aSearchResult, DBusMessage* aMsg) {
  DBusMessage* reply;
  char** stringArray;
  int elements;
  uint32_t timestamp;

  if (!dbus_message_get_args(aMsg, nullptr, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING,
                             &stringArray, &elements, DBUS_TYPE_UINT32,
                             &timestamp, DBUS_TYPE_INVALID) ||
      elements == 0) {
    reply = dbus_message_new_error(aMsg, GetDBusBusName(), "Wrong argument");
  } else {
    reply = dbus_message_new_method_return(aMsg);
    DBusLaunchWithAllResults(aSearchResult, timestamp);
    dbus_free_string_array(stringArray);
  }

  dbus_connection_send(aSearchResult->GetDBusConnection(), reply, nullptr);
  dbus_message_unref(reply);

  return DBUS_HANDLER_RESULT_HANDLED;
}

bool IsHistoryResultNodeURI(nsINavHistoryResultNode* aHistoryNode) {
  uint32_t type;
  nsresult rv = aHistoryNode->GetType(&type);
  if (NS_FAILED(rv) || type != nsINavHistoryResultNode::RESULT_TYPE_URI)
    return false;

  nsAutoCString title;
  rv = aHistoryNode->GetTitle(title);
  if (NS_SUCCEEDED(rv) && !title.IsEmpty()) {
    return true;
  }

  rv = aHistoryNode->GetUri(title);
  return NS_SUCCEEDED(rv) && !title.IsEmpty();
}
