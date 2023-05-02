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
var { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
var { FileUtils } = ChromeUtils.import("resource://gre/modules/FileUtils.jsm");
var { AppConstants } = ChromeUtils.import("resource://gre/modules/AppConstants.jsm");

(function () {
	// 起動時に他の窓がある（２窓目の）場合は抜ける
	let list = Services.wm.getEnumerator("navigator:browser");
	while (list.hasMoreElements()) { if (list.getNext() != window) return; }

	if (window.UCL) {
		window.UCL.destroy();
		delete window.UCL;
	}

	window.UCL = {
		AGENT_SHEET: Ci.nsIStyleSheetService.AGENT_SHEET,
		USER_SHEET: Ci.nsIStyleSheetService.USER_SHEET,
		AUTHOR_SHEET: Ci.nsIStyleSheetService.AUTHOR_SHEET,
		readCSS: {},

		getCSSFolder:function(){
			return OS.Path.join(Services.prefs.getStringPref("UserCSSLoader.FOLDER","") || OS.Path.join(OS.Constants.Path.profileDir, "chrome","CSS"),"a").slice( 0, -1 )
		}
		,
		getFocusedWindow: function () {
			let win = document.commandDispatcher.focusedWindow;
			if (!win || win == window) win = content;
			return win;
		},
		init: function () {
			
			document.getElementById('main-menubar').insertBefore(window.MozXULElement.parseXULToFragment(`
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
			`), document.getElementById('helpMenu'));

			document.getElementById("mainKeyset").appendChild(
				window.MozXULElement.parseXULToFragment(`
				<key id="usercssloader-rebuild-key" oncommand="window.UCL.rebuild();" key="R" modifiers="alt"/>
				`)
			);

			this.rebuild();
			this.initialized = true;
			window.addEventListener("unload", this, false);
		},
		uninit: function () {
			const dis = [];
			for (let x of Object.keys(this.readCSS)) {
				if (!this.readCSS[x].enabled)
					dis.push(x);
			}
			Services.prefs.setStringPref("UserCSSLoader.disabled_list", encodeURIComponent(dis.join("|")));
			window.removeEventListener("unload", this, false);
		},
		destroy: function () {
			var i = document.getElementById("usercssloader-menu");
			if (i) i.parentNode.removeChild(i);
			var i = document.getElementById("usercssloader-rebuild-key");
			if (i) i.parentNode.removeChild(i);
			this.uninit();
		},
		handleEvent: function (event) {
			switch (event.type) {
				case "unload": this.uninit(); break;
			}
		},
		rebuild: async function () {
			let l10n = new Localization(["browser/floorp.ftl"], true);
			let ext = /\.css$/i;
			let not = /\.uc\.css/i;

			let cssFolder = this.getCSSFolder()
			let cssList = await IOUtils.getChildren(cssFolder)

			for(let elem of cssList){
				let fileName = elem.replace(cssFolder,"")
				if (!ext.test(fileName) || not.test(fileName)) continue;
				let CSS = this.loadCSS(fileName,cssFolder);
				CSS.flag = true;
			}

			for (let leafName of Object.keys(this.readCSS)) {
				const CSS = this.readCSS[leafName];
				if (!CSS.flag) {
					CSS.enabled = false;
					delete this.readCSS[leafName];
				}
				delete CSS.flag;
				this.rebuildMenu(leafName);
			}
			if (this.initialized) {
				if (typeof (StatusPanel) !== "undefined")
					StatusPanel._label = l10n.formatValueSync("rebuild-complete");
				else
					XULBrowserWindow.statusTextField.label = l10n.formatValueSync("rebuild-complete");
			}
		},
		loadCSS: function (aFile,folder) {
			var CSS = this.readCSS[aFile];
			if (!CSS) {
				CSS = this.readCSS[aFile] = new CSSEntry(aFile,folder);
				if (decodeURIComponent(Services.prefs.getStringPref("UserCSSLoader.disabled_list")).indexOf(aFile) === -1) {
					CSS.enabled = true;
				}
			} else if (CSS.enabled) {
				CSS.enabled = true;
			}
			return CSS;
		},
		rebuildMenu: function (aLeafName) {
			var CSS = this.readCSS[aLeafName];
			var menuitem = document.getElementById("usercssloader-" + aLeafName);
			if (!CSS) {
				if (menuitem)
					menuitem.parentNode.removeChild(menuitem);
				return;
			}
			if (!menuitem) {
				menuitem = window.MozXULElement.parseXULToFragment(`
				<menuitem label="${aLeafName}" id="usercssloader-${aLeafName}" type="checkbox" autocheck="false" oncommand="UCL.toggle(\'${aLeafName}\')" onclick="UCL.itemClick(event);" class="usercssloader-item ${(CSS.SHEET == this.AGENT_SHEET ? "AGENT_SHEET" : CSS.SHEET == this.AUTHOR_SHEET ? "AUTHOR_SHEET" : "USER_SHEET")}"/>
				`).children[0]
				document.getElementById("usercssloader-menupopup").appendChild(menuitem);
			}
			menuitem.setAttribute("checked", CSS.enabled);
		},
		toggle: function (aLeafName) {
			var CSS = this.readCSS[aLeafName];
			if (!CSS) return;
			CSS.enabled = !CSS.enabled;
			this.rebuildMenu(aLeafName);
		},
		itemClick: function (event) {
			if (event.button == 0) return;

			event.preventDefault();
			event.stopPropagation();
			let label = event.currentTarget.getAttribute("label");

			if (event.button == 1) {
				this.toggle(label);
			}
			else if (event.button == 2) {
				closeMenus(event.target);
				this.edit(UCL.getCSSFolder +  label);
			}
		},
		openFolder: function () {
			FileUtils.File(UCL.getCSSFolder()).launch();
		},
		editUserCSS: function (aLeafName) {
			this.edit(OS.Path.join(OS.Constants.Path.profileDir, "chrome","a").slice( 0, -1 ) + aLeafName);
		},
		edit: function (aFile) {
			function openInEditor (){
				try {
					const editor = Services.prefs.getStringPref("view_source.editor.path");
					var UI = Cc["@mozilla.org/intl/scriptableunicodeconverter"].createInstance(Ci.nsIScriptableUnicodeConverter);
					UI.charset = window.navigator.platform.toLowerCase().indexOf("win") >= 0 ? "Shift_JIS" : "UTF-8";
					var path = UI.ConvertFromUnicode(aFile);
					var app = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
					app.initWithPath(editor);
					var process = Cc["@mozilla.org/process/util;1"].createInstance(Ci.nsIProcess);
					process.init(app);
					process.run(false, [path], 1);
		     	  } catch (e) { }
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
				const vscodePath = OS.Constants.Path.homeDir + "\\AppData\\Local\\Programs\\Microsoft VS Code\\code.exe";
				const isVSCodeInstalled = await OS.File.exists(vscodePath);
				if (isVSCodeInstalled) {
				  editorPath = vscodePath;
				}
			  } else {
				// check if gedit is installed
				const geditPath = "/usr/bin/gedit";
				const isGeditInstalled = await OS.File.exists(geditPath);
				if (isGeditInstalled) {
				  editorPath = geditPath;
				}
			  }
			  return editorPath;
			}
			
			let setPathPromise = new Promise(async (resolve) => {
			  if (editor == "") {
				textEditorPath.value = await getEditorPath();
				if (Services.prompt.prompt(null, l10n.formatValueSync("not-found-editor-path"), l10n.formatValueSync("set-pref-description"), textEditorPath, null, { value: false })) {
				  Services.prefs.setStringPref("view_source.editor.path", textEditorPath.value);
				}
			  }
			  resolve();
			});
			
			setPathPromise.then(() => {
			  openInEditor();
			});
		},
		create: async function (aLeafName) {
			let l10n = new Localization(["browser/floorp.ftl"], true);
			if (!aLeafName) aLeafName = prompt(l10n.formatValueSync("please-enter-filename"), dateFormat(new Date(), "%Y_%m%d_%H%M%S"));
			if (aLeafName) aLeafName = aLeafName.replace(/\s+/g, " ").replace(/[\\/:*?\"<>|]/g, "");
			if (!aLeafName || !/\S/.test(aLeafName)) return;
			if (!/\.css$/.test(aLeafName)) aLeafName += ".css";
			let file = UCL.getCSSFolder() +  aLeafName;
			await IOUtils.writeUTF8(file, "");
			this.edit(file);
		},
	};

	function CSSEntry(aFile,folder) {
		this.path = folder + aFile;
		this.leafName = aFile;
		this.lastModifiedTime = 1;
		this.SHEET = /^xul-|\.as\.css$/i.test(this.leafName) ?
			Ci.nsIStyleSheetService.AGENT_SHEET :
			/\.author\.css$/i.test(this.leafName) ?
				Ci.nsIStyleSheetService.AUTHOR_SHEET :
				Ci.nsIStyleSheetService.USER_SHEET;
	}
	CSSEntry.prototype = {
		sss: Cc["@mozilla.org/content/style-sheet-service;1"].getService(Ci.nsIStyleSheetService),
		_enabled: false,
		get enabled() {
			return this._enabled;
		},
		set enabled(isEnable) {
			this._enabled = isEnable
			let uri = Services.io.newFileURI(FileUtils.File(this.path));
			IOUtils.exists(this.path).then((value) => {
				if(value && isEnable){
					if (this.sss.sheetRegistered(uri, this.SHEET)) {
						IOUtils.stat(this.path).then((value) => {
							if(this.lastModifiedTime != value.lastModified){
								this.sss.unregisterSheet(uri, this.SHEET);
								this.sss.loadAndRegisterSheet(uri, this.SHEET);
							}
						})
					}else{
						this.sss.loadAndRegisterSheet(uri, this.SHEET);
					}
				}else{
					this.sss.unregisterSheet(uri, this.SHEET);
				}
			})
		},
	};

	(async () => {
		folderPath = UCL.getCSSFolder()
		if (!(await IOUtils.exists(folderPath))) {
			await IOUtils.makeDirectory(folderPath)
		}
		UCL.init();
	})()

	function dateFormat(date, format) {
		format = format.replace("%Y", ("000" + date.getFullYear()).substr(-4));
		format = format.replace("%m", ("0" + (date.getMonth() + 1)).substr(-2));
		format = format.replace("%d", ("0" + date.getDate()).substr(-2));
		format = format.replace("%H", ("0" + date.getHours()).substr(-2));
		format = format.replace("%M", ("0" + date.getMinutes()).substr(-2));
		format = format.replace("%S", ("0" + date.getSeconds()).substr(-2));
		return format;
	}
})();
