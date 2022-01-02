# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

; Strings for the optional extensions page. This is not in the locale
; directory so these strings are only translated to zh-CN.

!if "${AB_CD}" == "en-US"
LangString EXTENSIONS_PAGE_TITLE 0  "Install Optional Extensions"
LangString EXTENSIONS_PAGE_SUBTITLE 0  "$ExtensionRecommender recommends these extensions"
LangString OPTIONAL_EXTENSIONS_CHECKBOX_DESC 0  "Install &Extension:"
LangString OPTIONAL_EXTENSIONS_DESC 0  "You can add or remove these extensions at any time. Click the menu button and choose “Add-ons”."
!endif

!if "${AB_CD}" == "zh-CN"
LangString EXTENSIONS_PAGE_TITLE 0  "安装可选扩展"
LangString EXTENSIONS_PAGE_SUBTITLE 0  "$ExtensionRecommender 推荐安装以下扩展"
LangString OPTIONAL_EXTENSIONS_CHECKBOX_DESC 0  "安装扩展(&E):"
LangString OPTIONAL_EXTENSIONS_DESC 0  "您随时可以点击浏览器的菜单按钮并选择“附加组件”来添加或移除扩展。"
!endif