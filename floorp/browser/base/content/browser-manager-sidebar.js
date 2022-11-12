/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
if (Services.prefs.getPrefType("floorp.browser.sidebar2.customurl2") != 0) {
    if (Services.prefs.getStringPref(`floorp.browser.sidebar2.data`, "") == "{\"data\":{\"1\":{\"url\":\"floorp//bmt\",\"width\":600},\"2\":{\"url\":\"floorp//bookmarks\",\"width\":415},\"3\":{\"url\":\"floorp//history\",\"width\":415},\"4\":{\"url\":\"floorp//downloads\",\"width\":415},\"5\":{\"url\":\"floorp//tst\",\"width\":415},\"w1\":{\"url\":\"https://freasearch.org\"},\"w2\":{\"url\":\"https://translate.google.com\"}},\"index\":[\"1\",\"2\",\"3\",\"4\",\"5\",\"w1\",\"w2\"]}" || Services.prefs.getStringPref(`floorp.browser.sidebar2.data`, "") == "") {
        let updateObject = { "data": {}, "index": [] }
        let staticURL = ["floorp//bmt", "floorp//bookmarks", "floorp//history", "floorp//downloads", "floorp//tst"]
        for (let i = 0; i <= 4; i++) {
            let objectInObject = { "url": staticURL[i] }
            if (Services.prefs.getIntPref(`floorp.browser.sidebar2.width.mode${i}`, 0) != 0) {
                objectInObject["width"] = Services.prefs.getIntPref(`floorp.browser.sidebar2.width.mode${i}`, 0)
            } else {
                objectInObject["width"] = (i == 0 ? 600 : 415)
            }
            Services.prefs.clearUserPref(`floorp.browser.sidebar2.width.mode${i}`)
            updateObject.data[String(i)] = objectInObject
            updateObject.index.push(String(i))
        }
        let defaultURL = ["https://freasearch.org","https://translate.google.com"]
        for (let i = 0; i <= 19; i++) {
            let objectInObject = {}
            if (Services.prefs.getStringPref(`floorp.browser.sidebar2.customurl${i}`, "") != "" || i < 2) {
                objectInObject["url"] = Services.prefs.getStringPref(`floorp.browser.sidebar2.customurl${i}`, defaultURL[i] ?? "")

                if (Services.prefs.getIntPref(`floorp.browser.sidebar2.customurl${i}.usercontext`, 0) != 0) {
                    objectInObject["usercontext"] = Services.prefs.getIntPref(`floorp.browser.sidebar2.customurl${i}.usercontext`, 0)
                }

                if (Services.prefs.getPrefType(`floorp.browser.sidebar2.width.mode${i + 5}`) != 0 && Services.prefs.getIntPref(`floorp.browser.sidebar2.width.mode${i + 5}`, 0) != 0) {
                    objectInObject["width"] = Services.prefs.getIntPref(`floorp.browser.sidebar2.width.mode${i + 5}`, 0)
                    Services.prefs.clearUserPref(`floorp.browser.sidebar2.width.mode${i + 5}`)
                }
            }
            Services.prefs.clearUserPref(`floorp.browser.sidebar2.customurl${i}`)
            Services.prefs.clearUserPref(`floorp.browser.sidebar2.customurl${i}.usercontext`)
            Services.prefs.clearUserPref(`services.sync.prefs.sync.floorp.browser.sidebar2.customurl${i}`)
            Services.prefs.clearUserPref(`services.sync.prefs.sync.floorp.browser.sidebar2.customurl${i}.usercontext`)
            if (Object.keys(objectInObject).length != 0) {
                updateObject.data[`w${String(i)}`] = objectInObject
                updateObject.index.push(`w${String(i)}`)
            }
        }
        if(updateObject.index["floorp.browser.sidebar2.page"] != undefined) Services.prefs.setStringPref(`floorp.browser.sidebar2.page`,updateObject.index["floorp.browser.sidebar2.page"])
        Services.prefs.setStringPref(`floorp.browser.sidebar2.data`, JSON.stringify(updateObject))
    } else {
        for (let i = 0; i <= 4; i++) {
            if (Services.prefs.getPrefType(`floorp.browser.sidebar2.width.mode${i}`) != 0) {
                Services.prefs.clearUserPref(`floorp.browser.sidebar2.width.mode${i}`)
            }
        }
      for (let i = 0; i <= 19; i++) {
        if (Services.prefs.getPrefType(`floorp.browser.sidebar2.width.mode${i + 5}`) != 0) {
            Services.prefs.clearUserPref(`floorp.browser.sidebar2.width.mode${i + 5}`)
        }
        Services.prefs.clearUserPref(`floorp.browser.sidebar2.customurl${i}`)
        Services.prefs.clearUserPref(`floorp.browser.sidebar2.customurl${i}.usercontext`)
        Services.prefs.clearUserPref(`services.sync.prefs.sync.floorp.browser.sidebar2.customurl${i}`)
        Services.prefs.clearUserPref(`services.sync.prefs.sync.floorp.browser.sidebar2.customurl${i}.usercontext`)
        Services.prefs.clearUserPref(`floorp.browser.sidebar2.page`)
      }
    }
}

// 初回起動時、UA を変更するようにするための処理
if (Services.prefs.getBoolPref("floorp.browser.sidebar.enable", false)) {
    Services.prefs.addObserver("floorp.browser.sidebar2.page", function(){

if(Services.prefs.getStringPref("floorp.browser.sidebar2.page") != "") setSidebarMode();
 })};
Services.prefs.addObserver("floorp.browser.sidebar2.global.webpanel.width", setSidebarWidth)
const DEFAULT_STATIC_SIDEBAR_MODES_AMOUNT = 5 /* Static sidebar modes, that are unchangable by user. Starts from 0 */
const DEFAULT_DYNAMIC_CUSTOMURL_MODES_AMOUNT = 19 /* CustomURL modes, that are editable by user. Starts from 0 */
const STATIC_SIDEBAR_L10N_LIST = {
"floorp//bmt":`show-browser-manager-sidebar`,
"floorp//bookmarks":`show-bookmark-sidebar`,
"floorp//history":`show-history-sidebar`,
"floorp//downloads":`show-download-sidebar`,
"floorp//tst":`show-TST-sidebar`
}
let BROWSER_SIDEBAR_DATA = JSON.parse(Services.prefs.getStringPref(`floorp.browser.sidebar2.data`, undefined))
if (Services.prefs.getBoolPref("floorp.browser.sidebar.enable", false)) {
  Services.prefs.addObserver(`floorp.browser.sidebar2.data`, function() {
    BROWSER_SIDEBAR_DATA = JSON.parse(Services.prefs.getStringPref(`floorp.browser.sidebar2.data`, undefined))
    setSidebarIconView()
    setSidebarMode()
    setAllfavicons()
  })
 }

//startup functions
const webpanel_id_ = Services.prefs.getStringPref("floorp.browser.sidebar2.page", "");
setSidebarIconView();
if(webpanel_id_ != "" && BROWSER_SIDEBAR_DATA.index.indexOf(webpanel_id_) != -1) setSidebarMode();
removeAttributeSelectedNode();
if(webpanel_id_ != "" && BROWSER_SIDEBAR_DATA.index.indexOf(webpanel_id_) != -1) getSelectedNode().setAttribute("checked", "true");
setAllfavicons();
changeSidebarVisibility();