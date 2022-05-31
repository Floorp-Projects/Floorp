// ==UserScript==
// @name           zzzz-MultiRowTab_LiteforFx48.uc.js
// @namespace      http://space.geocities.yahoo.co.jp/gl/alice0775
// @description    多段タブもどき実験版 CSS入れ替えまくりLiteバージョン
// @include        main
// @compatibility  Firefox 101
// @author         Alice0775
// @version        2016/08/05 00:00 Firefox 48
// @version        2016/05/01 00:01 hide favicon if busy
// @version        2016/03/09 00:01 Bug 1222490 - Actually remove panorama for Fx45+
// @version        2016/02/09 00:01 workaround css for lwt
// @version        2016/02/09 00:00
// ==/UserScript==
"use strict";
MultiRowTabLiteforFx();
function MultiRowTabLiteforFx() {

    var css =` @-moz-document url-prefix("chrome://browser/content/browser.xhtml") {

    /* ツールバーの並び順 */
    #toolbar-menubar { -moz-box-ordinal-group: 1; } /* メニューバー */
    #nav-bar         { -moz-box-ordinal-group: 2; } /* ナビゲーションツールバー */
    #PersonalToolbar { -moz-box-ordinal-group: 3; } /* ブックマークツールバー */

    /* ツールバーの調整 */
    #titlebar,#tabbrowser-tabs { -moz-appearance: none !important; }
    #titlebar { border-top: 1px solid var(--chrome-content-separator-color) !important; }

    /* タイトルバーボタン[－□×]の調整 */
    #nav-bar > .titlebar-buttonbox-container .titlebar-button { width: 46px !important; }
    #toolbar-menubar:not([inactive]) ~ #nav-bar:not([inFullscreen]) > .titlebar-buttonbox-container { display: none !important; }

    /* フルスクリーン時タブバーを隠して画面の一番上と一番下をマウスオーバーすると表示する様にしてみました。
       画面の一番上をマウスオーバーした場合ツールバーと一緒にタブバーが出てきます。
       画面の一番下をマウスオーバーした場合タブバーだけ出てきます。 */
    #titlebar #TabsToolbar[inFullscreen] { max-height: 0 !important; }
    #navigator-toolbox-background:hover ~ #titlebar #TabsToolbar[inFullscreen],
    #titlebar:hover #TabsToolbar[inFullscreen] { max-height: 100% !important; }

    /* 多段タブ */
    box.scrollbox-clip[orient="horizontal"] { display: block !important; }
    box.scrollbox-clip > scrollbox[orient="horizontal"] {
        display: flex !important;
        flex-wrap: wrap !important;
        margin-bottom: 1px !important; }
    .tabbrowser-tab[fadein]:not([pinned]) { flex-grow: 1 !important; }
    .tabbrowser-tab,#tabs-newtab-button { height: calc(8px + var(--tab-min-height)); }
    .tabbrowser-tab > .tab-stack { width: 100% !important; }
    #tabs-newtab-button { margin: 0 !important; }

    /* 非表示 */
    .tabbrowser-tab:not([fadein]) { display: none !important; }

    /* --- タブバー ドラッグ領域 --- */

    /* 横幅 調整 */
    hbox.titlebar-spacer[type="pre-tabs"] { width: 0px !important; } /* 左のドラッグ領域：デフォルト 40px */
    hbox.titlebar-spacer[type="post-tabs"] { width: 0px !important; } /* 右のドラッグ領域：デフォルト 40px */

    /* ↓CSSコードの左右にあるコメントアウトを外してCSSコードを有効にするとウィンドウを最大化した時非表示になる左のドラッグ領域が表示出来ます。 */
    /* :root:not([sizemode="normal"]) hbox.titlebar-spacer[type="pre-tabs"] { display: block !important; } */

    /* ↓CSSコードの左右にあるコメントアウトを外してCSSコードを有効にするとフルスクリーンにした時非表示になる左右のドラッグ領域が表示出来ます。 */
    /* :root[inFullscreen] .titlebar-spacer { display: block !important; } */

    /* --- スクリプトを使用してタブバーをウィンドウの下部に移動する場合 --- */

    /* テーマが機能しない為、browser.cssから機能させる為に必要なCSSコードを取り込んで、#navigator-toolboxを#titlebarに変更して追加しました */
    #titlebar:-moz-lwtheme {
        background-image: var(--lwt-additional-images);
        background-repeat: var(--lwt-background-tiling);
        background-position: var(--lwt-background-alignment);
    }

    /* TODO bug 1695280: Remove these media selectors and merge the rule below
       with the ruleset above. We must set background properties on :root and not
       #navigator-toolbox on Windows 7/8 due to a WebRender bug that hides the
       minimize/maximize/close buttons. */
    @media not (-moz-os-version: windows-win7) {
        @media not (-moz-os-version: windows-win8) {
            #titlebar:-moz-lwtheme {
                background-color: var(--lwt-accent-color);
            }

            /* When a theme defines both theme_frame and additional_backgrounds, show
               the latter atop the former. */
            :root[lwtheme-image] #titlebar {
                background-image: var(--lwt-header-image), var(--lwt-additional-images);
                background-repeat: no-repeat, var(--lwt-background-tiling);
                background-position: right top, var(--lwt-background-alignment);
            }

            #titlebar:-moz-window-inactive:-moz-lwtheme {
                background-color: var(--lwt-accent-color-inactive, var(--lwt-accent-color));
            }
        }
    }

    } `;
    var sss = Cc['@mozilla.org/content/style-sheet-service;1'].getService(Ci.nsIStyleSheetService);
    var uri = makeURI('data:text/css;charset=UTF=8,' + encodeURIComponent(css));
    sss.loadAndRegisterSheet(uri, sss.USER_SHEET);

    if(location.href !== 'chrome://browser/content/browser.xhtml') return;

    // メニューバーをツールバーの上部に移動
    document.getElementById("titlebar").parentNode.insertBefore(document.getElementById("toolbar-menubar"),document.getElementById("nav-bar"));

    // タブバーをウィンドウの下部に移動
    document.body.appendChild(document.getElementById("titlebar"));

    // タブバーのタイトルバーボタン[－□×]をナビゲーションツールバーの右端に移動
    document.getElementById("nav-bar").appendChild(document.querySelector("#TabsToolbar .titlebar-buttonbox-container"));

    // タブバーshadowRoot内のスクロールボタンとスペーサーを非表示
    gBrowser.tabContainer.arrowScrollbox.shadowRoot.getElementById('scrollbutton-up').style.display = "none";
    gBrowser.tabContainer.arrowScrollbox.shadowRoot.getElementById('scrollbutton-down').style.display = "none";
    gBrowser.tabContainer.arrowScrollbox.shadowRoot.querySelector('[part="overflow-start-indicator"]').style.display = "none";
    gBrowser.tabContainer.arrowScrollbox.shadowRoot.querySelector('[part="overflow-end-indicator"]').style.display = "none";

    // Tabbar scrollIntoView
    gBrowser.tabContainer.addEventListener("dragend", function(event) {event.target.scrollIntoView({behavior: "instant", block: "nearest", inline: "nearest"})}, true);
    gBrowser.tabContainer.addEventListener("SSTabRestoring", function(event) {event.target.scrollIntoView({behavior: "instant", block: "nearest", inline: "nearest"})}, true);
    gBrowser.tabContainer.addEventListener("TabOpen", function(event) {event.target.scrollIntoView({behavior: "instant", block: "nearest", inline: "nearest"})}, true);
    gBrowser.tabContainer.addEventListener("TabSelect", function(event) {event.target.scrollIntoView({behavior: "instant", block: "nearest", inline: "nearest"})}, true);

    // drag & drop & DropIndicator

    gBrowser.tabContainer.clearDropIndicator = function() {
      let tabs = this.allTabs;
      for (let i = 0, len = tabs.length; i < len; i++) {
        tabs[i].removeAttribute("style");
      }
    }
    gBrowser.tabContainer.addEventListener("dragleave", function(event) { this.clearDropIndicator(event); }, true);

    gBrowser.tabContainer.on_dragover = function(event) {
      this.clearDropIndicator();
      var effects = this._getDropEffectForTabDrag(event);

      var ind = this._tabDropIndicator;
      if (effects == "" || effects == "none") {
        ind.hidden = true;
        return;
      }
      event.preventDefault();
      event.stopPropagation();

      var arrowScrollbox = this.arrowScrollbox;

      // autoscroll the tab strip if we drag over the scroll
      // buttons, even if we aren't dragging a tab, but then
      // return to avoid drawing the drop indicator
      var pixelsToScroll = 0;
      if (this.getAttribute("overflow") == "true") {
        switch (event.originalTarget) {
          case arrowScrollbox._scrollButtonUp:
            pixelsToScroll = arrowScrollbox.scrollIncrement * -1;
            break;
          case arrowScrollbox._scrollButtonDown:
            pixelsToScroll = arrowScrollbox.scrollIncrement;
            break;
        }
        if (pixelsToScroll) {
          arrowScrollbox.scrollByPixels(
            (RTL_UI ? -1 : 1) * pixelsToScroll,
            true
          );
        }
      }

      let draggedTab = this._getDropIndex(event, false); // event.dataTransfer.mozGetDataAt(TAB_DROP_TYPE, 0);
      if (
        (effects == "move" || effects == "copy") &&
        this == draggedTab.container
      ) {
        ind.hidden = true;

        if (!this._isGroupTabsAnimationOver()) {
          // Wait for grouping tabs animation to finish
          return;
        }
        this._finishGroupSelectedTabs(draggedTab);

        if (effects == "move") {
          this._animateTabMove(event);
          return;
        }
      }

      this._finishAnimateTabMove();

      if (effects == "link") {
        let tab = this._getDragTargetTab(event, true);
        if (tab) {
          if (!this._dragTime) {
            this._dragTime = Date.now();
          }
          if (Date.now() >= this._dragTime + this._dragOverDelay) {
            this.selectedItem = tab;
          }
          ind.hidden = true;
          return;
        }
      }

      var rect = arrowScrollbox.getBoundingClientRect();
      var newMargin;
      if (pixelsToScroll) {
        // if we are scrolling, put the drop indicator at the edge
        // so that it doesn't jump while scrolling
        let scrollRect = arrowScrollbox.scrollClientRect;
        let minMargin = scrollRect.left - rect.left;
        let maxMargin = Math.min(
          minMargin + scrollRect.width,
          scrollRect.right
        );
        if (RTL_UI) {
          [minMargin, maxMargin] = [
            this.clientWidth - maxMargin,
            this.clientWidth - minMargin,
          ];
        }
        newMargin = pixelsToScroll > 0 ? maxMargin : minMargin;
      } else {
        let newIndex = this._getDropIndex(event, effects == "link");
        let children = this.allTabs;
        if (newIndex == children.length) {
       // let tabRect = children[newIndex - 1].getBoundingClientRect();
          children[newIndex - 1].style.setProperty("box-shadow","-1px 0 0 red inset,1px 0 0 red","important");
          if (RTL_UI) {
            newMargin = rect.right - tabRect.left;
          } else {
            newMargin = tabRect.right - rect.left;
          }
        } else {
       // let tabRect = children[newIndex].getBoundingClientRect();
          children[newIndex].style.setProperty("box-shadow","1px 0 0 red inset,-1px 0 0 red","important");
          if (RTL_UI) {
            newMargin = rect.right - tabRect.right;
          } else {
            newMargin = tabRect.left - rect.left;
          }
        }
      }

      ind.hidden = false;
      newMargin += ind.clientWidth / 2;
      if (RTL_UI) {
        newMargin *= -1;
      }
      ind.style.transform = "translate(" + Math.round(newMargin) + "px)";
    }

    gBrowser.tabContainer.on_drop = function(event) {
      this.clearDropIndicator();
      var dt = event.dataTransfer;
      var dropEffect = dt.dropEffect;
      var draggedTab;
      let movingTabs;
      if (dt.mozTypesAt(0)[0] == TAB_DROP_TYPE) {
        // tab copy or move
        draggedTab = dt.mozGetDataAt(TAB_DROP_TYPE, 0);
        // not our drop then
        if (!draggedTab) {
          return;
        }
        movingTabs = draggedTab._dragData.movingTabs;
        draggedTab.container._finishGroupSelectedTabs(draggedTab);
      }

      this._tabDropIndicator.hidden = true;
      event.stopPropagation();
      if (draggedTab && dropEffect == "copy") {
        // copy the dropped tab (wherever it's from)
        let newIndex = this._getDropIndex(event, false);
        let draggedTabCopy;
        for (let tab of movingTabs) {
          let newTab = gBrowser.duplicateTab(tab);
          gBrowser.moveTabTo(newTab, newIndex++);
          if (tab == draggedTab) {
            draggedTabCopy = newTab;
          }
        }
        if (draggedTab.container != this || event.shiftKey) {
          this.selectedItem = draggedTabCopy;
        }
      } else if (draggedTab && draggedTab.container == this) {
        let oldTranslateX = Math.round(draggedTab._dragData.translateX);
        let tabWidth = Math.round(draggedTab._dragData.tabWidth);
        let translateOffset = oldTranslateX % tabWidth;
        let newTranslateX = oldTranslateX - translateOffset;
        if (oldTranslateX > 0 && translateOffset > tabWidth / 2) {
          newTranslateX += tabWidth;
        } else if (oldTranslateX < 0 && -translateOffset > tabWidth / 2) {
          newTranslateX -= tabWidth;
        }

        let dropIndex = this._getDropIndex(event, false);
       // "animDropIndex" in draggedTab._dragData &&
       // draggedTab._dragData.animDropIndex;
        let incrementDropIndex = true;
        if (dropIndex && dropIndex > movingTabs[0]._tPos) {
          dropIndex--;
          incrementDropIndex = false;
        }

        if (oldTranslateX && oldTranslateX != newTranslateX && !gReduceMotion) {
          for (let tab of movingTabs) {
            tab.setAttribute("tabdrop-samewindow", "true");
            tab.style.transform = "translateX(" + newTranslateX + "px)";
            let postTransitionCleanup = () => {
              tab.removeAttribute("tabdrop-samewindow");

              this._finishAnimateTabMove();
              if (dropIndex !== false) {
                gBrowser.moveTabTo(tab, dropIndex);
                if (incrementDropIndex) {
                  dropIndex++;
                }
              }

              gBrowser.syncThrobberAnimations(tab);
            };
            if (gReduceMotion) {
              postTransitionCleanup();
            } else {
              let onTransitionEnd = transitionendEvent => {
                if (
                  transitionendEvent.propertyName != "transform" ||
                  transitionendEvent.originalTarget != tab
                ) {
                  return;
                }
                tab.removeEventListener("transitionend", onTransitionEnd);

                postTransitionCleanup();
              };
              tab.addEventListener("transitionend", onTransitionEnd);
            }
          }
        } else {
          this._finishAnimateTabMove();
          if (dropIndex !== false) {
            for (let tab of movingTabs) {
              gBrowser.moveTabTo(tab, dropIndex);
              if (incrementDropIndex) {
                dropIndex++;
              }
            }
          }
        }
      } else if (draggedTab) {
        // Move the tabs. To avoid multiple tab-switches in the original window,
        // the selected tab should be adopted last.
        const dropIndex = this._getDropIndex(event, false);
        let newIndex = dropIndex;
        let selectedTab;
        let indexForSelectedTab;
        for (let i = 0; i < movingTabs.length; ++i) {
          const tab = movingTabs[i];
          if (tab.selected) {
            selectedTab = tab;
            indexForSelectedTab = newIndex;
          } else {
            const newTab = gBrowser.adoptTab(tab, newIndex, tab == draggedTab);
            if (newTab) {
              ++newIndex;
            }
          }
        }
        if (selectedTab) {
          const newTab = gBrowser.adoptTab(
            selectedTab,
            indexForSelectedTab,
            selectedTab == draggedTab
          );
          if (newTab) {
            ++newIndex;
          }
        }

        // Restore tab selection
        gBrowser.addRangeToMultiSelectedTabs(
          gBrowser.tabs[dropIndex],
          gBrowser.tabs[newIndex - 1]
        );
      } else {
        // Pass true to disallow dropping javascript: or data: urls
        let links;
        try {
          links = browserDragAndDrop.dropLinks(event, true);
        } catch (ex) {}

        if (!links || links.length === 0) {
          return;
        }

        let inBackground = Services.prefs.getBoolPref(
          "browser.tabs.loadInBackground"
        );
        if (event.shiftKey) {
          inBackground = !inBackground;
        }

        let targetTab = this._getDragTargetTab(event, true);
        let userContextId = this.selectedItem.getAttribute("usercontextid");
        let replace = !!targetTab;
        let newIndex = this._getDropIndex(event, true);
        let urls = links.map(link => link.url);
        let csp = browserDragAndDrop.getCSP(event);
        let triggeringPrincipal = browserDragAndDrop.getTriggeringPrincipal(
          event
        );

        (async () => {
          if (
            urls.length >=
            Services.prefs.getIntPref("browser.tabs.maxOpenBeforeWarn")
          ) {
            // Sync dialog cannot be used inside drop event handler.
            let answer = await OpenInTabsUtils.promiseConfirmOpenInTabs(
              urls.length,
              window
            );
            if (!answer) {
              return;
            }
          }

          gBrowser.loadTabs(urls, {
            inBackground,
            replace,
            allowThirdPartyFixup: true,
            targetTab,
            newIndex,
            userContextId,
            triggeringPrincipal,
            csp,
          });
        })();
      }

      if (draggedTab) {
        delete draggedTab._dragData;
      }
    }

    gBrowser.tabContainer._getDropIndex = function(event, isLink) {
      var tabs = this.allTabs;
      var tab = this._getDragTargetTab(event, isLink);
      if (!RTL_UI) {
        for (let i = tab ? tab._tPos : 0; i < tabs.length; i++) {
          if (
            event.screenY <
            tabs[i].screenY + tabs[i].getBoundingClientRect().height
          ) {
            if (
              event.screenX <
              tabs[i].screenX + tabs[i].getBoundingClientRect().width / 2
            ) {
              return i;
            }
            if (
              event.screenX >
              tabs[i].screenX + tabs[i].getBoundingClientRect().width / 2 &&
              event.screenX <
              tabs[i].screenX + tabs[i].getBoundingClientRect().width
            ) {
              return i + 1;
            }
          }
        }
      } else {
        for (let i = tab ? tab._tPos : 0; i < tabs.length; i++) {
          if (
            event.screenY <
            tabs[i].screenY + tabs[i].getBoundingClientRect().height
          ) {
            if (
              event.screenX <
              tabs[i].screenX + tabs[i].getBoundingClientRect().width &&
              event.screenX >
              tabs[i].screenX + tabs[i].getBoundingClientRect().width / 2
            ) {
              return i;
            }
            if (
              event.screenX <
              tabs[i].screenX + tabs[i].getBoundingClientRect().width / 2
            ) {
              return i + 1;
            }
          }
        }
      }
      return tabs.length;
    }

}
