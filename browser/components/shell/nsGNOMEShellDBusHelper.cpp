/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGNOMEShellSearchProvider.h"
#include "nsGNOMEShellDBusHelper.h"

#include "nsPrintfCString.h"
#include "RemoteUtils.h"
#include "nsServiceManagerUtils.h"

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
  CopyASCIItoUTF16(nsCString(aSearchedTerm), *formatStrings.AppendElement());

  nsAutoString gnomeSearchTitle;
  bundle->FormatStringFromName("gnomeSearchProviderSearch", formatStrings,
                               gnomeSearchTitle);
  AppendUTF16toUTF8(gnomeSearchTitle, aGnomeSearchTitle);
  return true;
}

static const char* introspect_template =
    "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection "
    "1.0//EN\"\n"
    "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\";>\n"
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

DBusHandlerResult DBusHandleInitialResultSet(
    RefPtr<nsGNOMEShellHistorySearchResult> aSearchResult, DBusMessage* aMsg) {
  DBusMessage* reply;
  char** stringArray = nullptr;
  int elements;

  if (!dbus_message_get_args(aMsg, nullptr, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING,
                             &stringArray, &elements, DBUS_TYPE_INVALID) ||
      elements == 0) {
    reply = dbus_message_new_error(aMsg, DBUS_BUS_NAME, "Wrong argument");
    dbus_connection_send(aSearchResult->GetDBusConnection(), reply, nullptr);
    dbus_message_unref(reply);
  } else {
    aSearchResult->SetReply(dbus_message_new_method_return(aMsg));
    aSearchResult->SetSearchTerm(stringArray[0]);
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
    reply = dbus_message_new_error(aMsg, DBUS_BUS_NAME, "Wrong argument");
    dbus_connection_send(aSearchResult->GetDBusConnection(), reply, nullptr);
    dbus_message_unref(reply);
  } else {
    aSearchResult->SetReply(dbus_message_new_method_return(aMsg));
    aSearchResult->SetSearchTerm(stringArray[0]);
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
    nsCOMPtr<nsINavHistoryContainerResultNode> aHistResultContainer,
    DBusMessageIter* aIter, const char* aID) {
  nsCOMPtr<nsINavHistoryResultNode> child;
  aHistResultContainer->GetChild(DBusGetIndexFromIDKey(aID),
                                 getter_AddRefs(child));
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
  appendStringDictionary(aIter, "gicon", "text-html");
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
    appendStringDictionary(aIter, "gicon", "org.mozilla.Firefox");
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
    reply = dbus_message_new_error(aMsg, DBUS_BUS_NAME, "Wrong argument");
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
        DBusAppendResultID(aSearchResult->GetSearchResultContainer(),
                           &iterArray2, stringArray[i]);
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
    commandLine = ConstructCommandLine(3, (char**)urlList, nullptr, &tmp);
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
    commandLine = ConstructCommandLine(2, (char**)urlList, nullptr, &tmp);
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
    reply = dbus_message_new_error(aMsg, DBUS_BUS_NAME, "Wrong argument");
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
    reply = dbus_message_new_error(aMsg, DBUS_BUS_NAME, "Wrong argument");
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
