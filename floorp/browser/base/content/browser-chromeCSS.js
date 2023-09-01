/* eslint-disable no-undef */
// ==UserScript==
// @name           UserCSSLoader
// @description    Stylish みたいなもの
// @namespace      http://d.hatena.ne.jp/Griever/
// @author         Griever
// @include        main
// @license        MIT License
// @compatibility  Firefox 4
// @charset        UTF-8
// @version        0.0.4r3
// @note           0.0.4r3 Fx92: getURLSpecFromFile() -> getURLSpecFromActualFile()
// @note           0.0.4r2 ファイル名末尾.author.cssでAUTHOR_SHEETで読めるようにした
// @note           0.0.4 Remove E4X
// @note           CSSEntry クラスを作った
// @note           スタイルのテスト機能を作り直した
// @note           ファイルが削除された場合 rebuild 時に CSS を解除しメニューを消すようにした
// @note           uc で読み込まれた .uc.css の再読み込みに仮対応
// ==/UserScript==

/****** 使い方 ******

chrome フォルダに CSS フォルダが作成されるのでそこに .css をぶち込むだけ。
ファイル名が "xul-" で始まる物、".as.css" で終わる物は AGENT_SHEET で、
".author.css"で終わるものは AUTHOR_SHEETで、それ以外は USER_SHEET で読み込む。
ファイルの内容はチェックしないので @namespace 忘れに注意。

メニューバーに CSS メニューが追加される
メニューを左クリックすると ON/OFF
		  中クリックするとメニューを閉じずに ON/OFF
		  右クリックするとエディタで開く

エディタは "view_source.editor.path" に指定されているものを使う
フォルダは "UserCSSLoader.FOLDER" にパスを入れれば変更可能

 **** 説明終わり ****/
var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
var { FileUtils } = ChromeUtils.import("resource://gre/modules/FileUtils.jsm");
var { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

// プロファイルフォルダのパス
const PROFILE_DIR = Services.dirsvc.get("ProfD", Ci.nsIFile).path;

(function () {
  // 起動時に他の窓がある（２窓目の）場合は抜ける
  let list = Services.wm.getEnumerator("navigator:browser");
  while (list.hasMoreElements()) {
    if (list.getNext() != window) {
      return;
    }
  }

  if (window.UCL) {
    window.UCL.destroy();
    delete window.UCL;
  }

  window.UCL = {
    AGENT_SHEET: Ci.nsIStyleSheetService.AGENT_SHEET,
    USER_SHEET: Ci.nsIStyleSheetService.USER_SHEET,
    AUTHOR_SHEET: Ci.nsIStyleSheetService.AUTHOR_SHEET,
    readCSS: {},

    getCSSFolder() {
      let result = PathUtils.join(
        Services.prefs.getStringPref("UserCSSLoader.FOLDER", "") ||
          PathUtils.join(Services.dirsvc.get("ProfD", Ci.nsIFile).path, "chrome", "CSS"),
        "a"
      ).slice(0, -1);
      return result;
    },
    getFocusedWindow() {
      let win = document.commandDispatcher.focusedWindow;
      if (!win || win == window) {
        win = content;
      }
      return win;
    },
    init() {
      document.getElementById("main-menubar").insertBefore(
        window.MozXULElement.parseXULToFragment(`
			<menu data-l10n-id="css-menubar" id="usercssloader-menu">
				<menupopup id="usercssloader-menupopup">
					<menu data-l10n-id="css-menu">
						<menupopup id="usercssloader-submenupopup">
							<menuitem data-l10n-id="rebuild-css" acceltext="Alt + R" oncommand="window.UCL.rebuild();"/>
							<menuseparator />
							<menuitem data-l10n-id="make-browsercss-file" oncommand="window.UCL.create()"/>
							<menuitem data-l10n-id="open-css-folder" oncommand="window.UCL.openFolder();" />
							<menuitem data-l10n-id="edit-userChromeCss-editor" oncommand="window.UCL.editUserCSS(\'userChrome.css\');" />
							<menuitem data-l10n-id="edit-userContentCss-editor" oncommand="window.UCL.editUserCSS(\'userContent.css\');" />
						</menupopup>
					</menu>
				</menupopup>
			</menu>
			`),
        document.getElementById("helpMenu")
      );

      document.getElementById("mainKeyset").appendChild(
        window.MozXULElement.parseXULToFragment(`
				<key id="usercssloader-rebuild-key" oncommand="window.UCL.rebuild();" key="R" modifiers="alt"/>
				`)
      );

      this.rebuild();
      this.initialized = true;
      window.addEventListener("unload", this);
    },
    uninit() {
      const dis = [];
      for (let x of Object.keys(this.readCSS)) {
        if (!this.readCSS[x].enabled) {
          dis.push(x);
        }
      }
      Services.prefs.setStringPref(
        "UserCSSLoader.disabled_list",
        encodeURIComponent(dis.join("|"))
      );
      window.removeEventListener("unload", this);
    },
    destroy() {
      const i = document.getElementById("usercssloader-menu");
      if (i) {
        i.remove();
      }
      const j = document.getElementById("usercssloader-rebuild-key");
      if (j) {
        j.remove();
      }
      this.uninit();
    },
    handleEvent(event) {
      switch (event.type) {
        case "unload":
          this.uninit();
          break;
      }
    },
    async rebuild() {
      const l10n = new Localization(["browser/floorp.ftl"], true);
      const ext = /\.css$/i;
      const not = /\.uc\.css/i;

      const cssFolder = this.getCSSFolder();
      const cssList = await IOUtils.getChildren(cssFolder);

      for (const elem of cssList) {
        const fileName = elem.replace(cssFolder, "");
        if (!ext.test(fileName) || not.test(fileName)) {
          continue;
        }
        const CSS = this.loadCSS(fileName, cssFolder);
        CSS.flag = true;
      }

      for (const leafName of Object.keys(this.readCSS)) {
        const CSS = this.readCSS[leafName];
        if (!CSS.flag) {
          CSS.enabled = false;
          delete this.readCSS[leafName];
        }
        delete CSS.flag;
        this.rebuildMenu(leafName);
      }
      if (this.initialized) {
        if (typeof StatusPanel !== "undefined") {
          StatusPanel._label = l10n.formatValueSync("rebuild-complete");
        } else {
          XULBrowserWindow.statusTextField.label =
            l10n.formatValueSync("rebuild-complete");
        }
      }
    },
    loadCSS(aFile, folder) {
      let CSS = this.readCSS[aFile];
      if (!CSS) {
        CSS = this.readCSS[aFile] = new CSSEntry(aFile, folder);
        CSS.enabled = !decodeURIComponent(
          Services.prefs.getStringPref("UserCSSLoader.disabled_list","")
        ).includes(aFile);
      } else if (CSS.enabled) {
        CSS.enabled = true;
      }
      return CSS;
    },
    rebuildMenu(aLeafName) {
      var CSS = this.readCSS[aLeafName];
      var menuitem = document.getElementById("usercssloader-" + aLeafName);
      if (!CSS) {
        if (menuitem) {
          menuitem.remove();
        }
        return;
      }
      if (!menuitem) {
        menuitem = window.MozXULElement.parseXULToFragment(`
				<menuitem label="${aLeafName}" id="usercssloader-${aLeafName}" type="checkbox" autocheck="false" oncommand="UCL.toggle(\'${aLeafName}\')" onclick="UCL.itemClick(event);" class="usercssloader-item ${
          CSS.SHEET == this.AGENT_SHEET
            ? "AGENT_SHEET"
            : CSS.SHEET == this.AUTHOR_SHEET
            ? "AUTHOR_SHEET"
            : "USER_SHEET"
        }"/>
				`).children[0];
        document
          .getElementById("usercssloader-menupopup")
          .appendChild(menuitem);
      }
      menuitem.setAttribute("checked", CSS.enabled);
    },
    toggle(aLeafName) {
      var CSS = this.readCSS[aLeafName];
      if (!CSS) {
        return;
      }
      CSS.enabled = !CSS.enabled;
      this.rebuildMenu(aLeafName);
    },
    itemClick(event) {
      if (event.button == 0) {
        return;
      }

      event.preventDefault();
      event.stopPropagation();
      let label = event.currentTarget.getAttribute("label");

      if (event.button == 1) {
        this.toggle(label);
      } else if (event.button == 2) {
        closeMenus(event.target);
        this.edit(UCL.getCSSFolder() + label);//
      }
    },
    openFolder() {
      FileUtils.File(UCL.getCSSFolder()).launch();
    },
    editUserCSS(aLeafName) {
      this.edit(
        PathUtils.join(Services.dirsvc.get("ProfD", Ci.nsIFile).path, "chrome", "a").slice(
          0,
          -1
        ) + aLeafName
      );
    },
    edit(aFile) {
      function openInEditor() {
        try {
          const editor = Services.prefs.getStringPref(
            "view_source.editor.path"
          );
          var path = AppConstants.platform == "win" ? convertUTF8ToShiftJIS(aFile) : convertShiftJISToUTF8(aFile);
          var app = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
          app.initWithPath(editor);
          var process = Cc["@mozilla.org/process/util;1"].createInstance(
            Ci.nsIProcess
          );
          process.init(app);
          process.run(false, [path], 1);
        } catch (e) {
          throw new Error(e);
        }
      }

      function convertShiftJISToUTF8(shiftJISString) {
        const decoder = new TextDecoder('shift-jis');
        const shiftJISBytes = new TextEncoder().encode(shiftJISString);
        const utf8String = decoder.decode(shiftJISBytes);
        return utf8String;
      }

      function convertUTF8ToShiftJIS(utf8String) {
        const decoder = new TextDecoder('utf-8');
        const utf8Bytes = new TextEncoder().encode(utf8String);
        const shiftJISString = decoder.decode(utf8Bytes);
        return shiftJISString;
      }
      

      let l10n = new Localization(["browser/floorp.ftl"], true);
      const editor = Services.prefs.getStringPref("view_source.editor.path");
      let textEditorPath = { value: "" };

      async function getEditorPath() {
        let editorPath = "";
        if (AppConstants.platform == "win") {
          const notepadPath = "C:\\windows\\system32\\notepad.exe";
          editorPath = notepadPath;
          // check if VSCode is installed
          const vscodePath =
          Services.dirsvc.get("Home", Ci.nsIFile).path +
            "\\AppData\\Local\\Programs\\Microsoft VS Code\\code.exe";
          const isVSCodeInstalled = await IOUtils.exists(vscodePath);
          if (isVSCodeInstalled) {
            editorPath = vscodePath;
          }
        } else {
          // check if gedit is installed
          const geditPath = "/usr/bin/gedit";
          const isGeditInstalled = await IOUtils.exists(geditPath);
          if (isGeditInstalled) {
            editorPath = geditPath;
          }
        }
        return editorPath;
      }

      let setPathPromise = new Promise(resolve => {
        if (editor == "") {
          getEditorPath().then(path => {
            textEditorPath.value = path;
            if (
              Services.prompt.prompt(
                null,
                l10n.formatValueSync("not-found-editor-path"),
                l10n.formatValueSync("set-pref-description"),
                textEditorPath,
                null,
                { value: false }
              )
            ) {
              Services.prefs.setStringPref(
                "view_source.editor.path",
                textEditorPath.value
              );
            }
            resolve();
          });
        } else {
          resolve();
        }
      });

      setPathPromise.then(() => {
        openInEditor();
      });
    },
    async create(aLeafName) {
      let l10n = new Localization(["browser/floorp.ftl"], true);
      if (!aLeafName) {
        aLeafName = prompt(
          l10n.formatValueSync("please-enter-filename"),
          new Date().getTime()
        );
      }
      if (aLeafName) {
        aLeafName = aLeafName
          .replace(/\s+/g, " ")
          .replace(/[\\/:*?\"<>|]/g, "");
      }
      if (!aLeafName || !/\S/.test(aLeafName)) {
        return;
      }
      if (!/\.css$/.test(aLeafName)) {
        aLeafName += ".css";
      }
      let file = UCL.getCSSFolder() + aLeafName;
      await IOUtils.writeUTF8(file, "");
      this.edit(file);
    },
  };

  function CSSEntry(aFile, folder) {
    this.path = folder + aFile;
    this.leafName = aFile;
    this.lastModifiedTime = 1;
    if (/^xul-|\.as\.css$/i.test(this.leafName)) {
      this.SHEET = Ci.nsIStyleSheetService.AGENT_SHEET;
    } else if (/\.author\.css$/i.test(this.leafName)) {
      this.SHEET = Ci.nsIStyleSheetService.AUTHOR_SHEET;
    } else {
      this.SHEET = Ci.nsIStyleSheetService.USER_SHEET;
    }
  }
  CSSEntry.prototype = {
    sss: Cc["@mozilla.org/content/style-sheet-service;1"].getService(
      Ci.nsIStyleSheetService
    ),
    _enabled: false,
    get enabled() {
      return this._enabled;
    },
    set enabled(isEnable) {
      this._enabled = isEnable;
      let uri = Services.io.newFileURI(FileUtils.File(this.path));
      IOUtils.exists(this.path).then(value => {
        if (value && isEnable) {
          if (this.sss.sheetRegistered(uri, this.SHEET)) {
            IOUtils.stat(this.path).then(value => {
              if (this.lastModifiedTime != value.lastModified) {
                this.sss.unregisterSheet(uri, this.SHEET);
                this.sss.loadAndRegisterSheet(uri, this.SHEET);
              }
            });
          } else {
            this.sss.loadAndRegisterSheet(uri, this.SHEET);
          }
        } else {
          this.sss.unregisterSheet(uri, this.SHEET);
        }
      });
    },
  };

  (async () => {
    folderPath = UCL.getCSSFolder();
    if (!(await IOUtils.exists(folderPath))) {
      await IOUtils.makeDirectory(folderPath);
    }
    UCL.init();
  })();
})();
