g_SelectStr = ""; // 検索文字列
g_IsImage = false; // 画像かどうかのフラグ
g_IsBase64 = false; // Base64でエンコードされているか
g_IsAddressSearch = false; // Webアドレス検索かどうか(true:Webアドレス検索、false:通常検索)
g_settingEngineURL = "https://www.google.com/search?q="; // 検索エンジン文字列
g_settingNewTabPosition = "right"; // 新規にタブを開く位置
g_settingIsAddressForground = true; // Webアドレスをフォアグラウンドタブで開くかどうか
g_settingIsSearchForground = true; // 検索結果をフォアグラウンドタブで開くかどうか
g_settingIsSaveImage = true; // ドラッグ＆ドロップで画像を保存するかどうか
g_settingIsPreferSaveImage = true; // リンク付き画像の場合、画像保存を優先するかどうか

// RFC3986判定
function isRFC3986(str) {
	var hasRFC3986 = /^[a-z]([a-z]|[0-9]|[+\-.])*:(\/\/((([a-z]|[0-9]|[-._~])|%[0-9a-f][0-9a-f]|[!$&'()*+,;=]|:)*@)?(\[((([0-9a-f]{1,4}:){6}([0-9a-f]{1,4}:[0-9a-f]{1,4}|([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])(\.([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])){3})|::([0-9a-f]{1,4}:){5}([0-9a-f]{1,4}:[0-9a-f]{1,4}|([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])(\.([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])){3})|([0-9a-f]{1,4})?::([0-9a-f]{1,4}:){4}([0-9a-f]{1,4}:[0-9a-f]{1,4}|([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])(\.([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])){3})|(([0-9a-f]{1,4}:){0,1}[0-9a-f]{1,4})?::([0-9a-f]{1,4}:){3}([0-9a-f]{1,4}:[0-9a-f]{1,4}|([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])(\.([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])){3})|(([0-9a-f]{1,4}:){0,2}[0-9a-f]{1,4})?::([0-9a-f]{1,4}:){2}([0-9a-f]{1,4}:[0-9a-f]{1,4}|([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])(\.([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])){3})|(([0-9a-f]{1,4}:){0,3}[0-9a-f]{1,4})?::[0-9a-f]{1,4}:([0-9a-f]{1,4}:[0-9a-f]{1,4}|([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])(\.([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])){3})|(([0-9a-f]{1,4}:){0,4}[0-9a-f]{1,4})?::([0-9a-f]{1,4}:[0-9a-f]{1,4}|([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])(\.([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])){3})|(([0-9a-f]{1,4}:){0,5}[0-9a-f]{1,4})?::[0-9a-f]{1,4}|(([0-9a-f]{1,4}:){0,6}[0-9a-f]{1,4})?::)|v[0-9a-f]+\.(([a-z]|[0-9]|[-._~])|[!$&'()*+,;=]|:)+)]|([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])(\.([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])){3}|(([a-z]|[0-9]|[-._~])|%[0-9a-f][0-9a-f]|[!$&'()*+,;=])*)(:\d*)?(\/((([a-z]|[0-9]|[-._~])|%[0-9a-f][0-9a-f]|[!$&'()*+,;=]|[:@]))*)*|\/(((([a-z]|[0-9]|[-._~])|%[0-9a-f][0-9a-f]|[!$&'()*+,;=]|[:@]))+(\/((([a-z]|[0-9]|[-._~])|%[0-9a-f][0-9a-f]|[!$&'()*+,;=]|[:@]))*)*)?|((([a-z]|[0-9]|[-._~])|%[0-9a-f][0-9a-f]|[!$&'()*+,;=]|[:@]))+(\/((([a-z]|[0-9]|[-._~])|%[0-9a-f][0-9a-f]|[!$&'()*+,;=]|[:@]))*)*|)(\?((([a-z]|[0-9]|[-._~])|%[0-9a-f][0-9a-f]|[!$&'()*+,;=]|[:@])|[\/?])*)?(#((([a-z]|[0-9]|[-._~])|%[0-9a-f][0-9a-f]|[!$&'()*+,;=]|[:@])|[\/?])*)?$/i;
	return hasRFC3986.test(str);
}

// QuickDragでの特殊URL判定
function isSpecialURL(str) {
	var isURI = false;
	var hasDomain = new RegExp(
		"(?:^|[:\\/\\.@])" +
		"[a-z0-9](?:[a-z0-9-]*[a-z0-9])" +
		"\\.(?:(?!js)[a-z]{2}|com|net|org|info|biz|name|pro|aero|coop|museum|jobs|travel|mail|cat|post|asia|mobi|tel|xxx|int|gov|mil|edu|arpa|example|invalid|localhost|test|onion)" +
		"(?:[:\\/\\?]|$)",
		"i"
	);
	isURI = isURI || (!/\s/.test(str) && hasDomain.test(str));
	return isURI;
}

// 設定パラメータ更新 (検索エンジン)
function updateParamEngine(storage_data) {
	g_settingEngineURL = storage_data;
}

// 設定パラメータ更新 (新規タブ位置)
function updateNewTabPosition(storage_data) {
	g_settingNewTabPosition = storage_data;
}

// 設定パラメータ更新 (チェックボックス)
function updateParamcheckboxArray(storage_data) {
	g_settingIsAddressForground = storage_data.indexOf("is_address_forground") >= 0 ? true : false;
	g_settingIsSearchForground = storage_data.indexOf("is_search_forground") >= 0 ? true : false;
	g_settingIsSaveImage = storage_data.indexOf("is_save_image") >= 0 ? true : false;
	g_settingIsPreferSaveImage = storage_data.indexOf("is_prefer_save_image") >= 0 ? true : false;
}

// デフォルトイベントを無効化
function eventInvalid(e) {
	if (e.preventDefault && false === e.shiftKey) {
		e.preventDefault();
	}
}

// メッセージ受信
function receiveMessage(e) {
	if (null != e.data.message_addon) {
		if ("quickdrag_we_set_str" === e.data.message_addon) {
			g_SelectStr = e.data.SelectStr;
			g_IsImage = e.data.IsImage;
			g_IsBase64 = e.data.IsBase64;
			g_IsAddressSearch = e.data.IsAddressSearch;
		}
	}
}

// メッセージ送信
function sendMessage(send_data, is_image, is_base64, is_address_search) {
	if (window !== window.top) {
		// ドラッグ先のウインドウがトップウインドウでない場合
		window.top.postMessage({
			message_addon: "quickdrag_we_set_str",
			SelectStr: send_data,
			IsImage: is_image,
			IsBase64: is_base64,
			IsAddressSearch: is_address_search
		}, '*');
		for (var i = 0; i < window.top.length; i++) {
			window.top[i].postMessage({
				message_addon: "quickdrag_we_set_str",
				SelectStr: send_data,
				IsImage: is_image,
				IsBase64: is_base64,
				IsAddressSearch: is_address_search
			}, '*');
		}
	} else {
		// トップウインドウの場合
		var frames = document.getElementsByTagName('frame');
		for (var i = 0; i < frames.length; i++) {
			frames[i].contentWindow.postMessage({
				message_addon: "quickdrag_we_set_str",
				SelectStr: send_data,
				IsImage: is_image,
				IsBase64: is_base64,
				IsAddressSearch: is_address_search
			}, '*');
		}

		var iframes = document.getElementsByTagName('iframe');
		for (var i = 0; i < iframes.length; i++) {
			iframes[i].contentWindow.postMessage({
				message_addon: "quickdrag_we_set_str",
				SelectStr: send_data,
				IsImage: is_image,
				IsBase64: is_base64,
				IsAddressSearch: is_address_search
			}, '*');
		}
	}
}

// ドラッグ開始
function handleDragStart(e) {
	g_IsImage = false;
	g_IsBase64 = false;
	g_IsAddressSearch = false;
	g_SelectStr = "";

	if(true === e.shiftKey) {
		return;
	}

	if ("[object HTMLImageElement]" === e.explicitOriginalTarget.toString()) {
		if (void 0 === e.target.href || true === g_settingIsPreferSaveImage) {
			g_IsImage = true;
			g_SelectStr = e.explicitOriginalTarget.src.toString();
			var hasScheme = /^(?:(?:( +)?h?tt|hxx)ps?|ftp|chrome|file):\/\//i;
			if (false === hasScheme.test(g_SelectStr)) {
				g_IsBase64 = true;
			}
		} else {
			g_IsAddressSearch = true;
			g_SelectStr = e.target.href;
		}
	} else {
		if (true === isRFC3986(e.dataTransfer.getData("text/plain").replace(/^ +/i, ""))) {
			g_IsAddressSearch = true;
			g_SelectStr = e.dataTransfer.getData("text/plain").replace(/^ +/i, "");
			g_SelectStr = g_SelectStr.replace(/^(?:t?t|h[tx]{2,})p(s?:\/\/)/i, "http$1");
		} else if (true === isSpecialURL(e.dataTransfer.getData("text/plain").replace(/^ +/i, ""))) {
			g_IsAddressSearch = true;
			g_SelectStr = e.dataTransfer.getData("text/plain").replace(/^ +/i, "");
			if (/^[\w\.\+\-]+@[\w\.\-]+\.[\w\-]{2,}$/.test(g_SelectStr))
				g_SelectStr = "mailto:" + g_SelectStr;

			if (!/^[a-z][\da-z+\-]*:/i.test(g_SelectStr))
				g_SelectStr = g_SelectStr.replace(/^:*[\/\\\s]*/, "http://").replace(/^ht(tp:\/\/ftp\.)/i, "f$1");
		} else {
			g_SelectStr = encodeURIComponent(e.dataTransfer.getData("text/plain"));
			var replace_select_str = g_settingEngineURL.replace("%s", g_SelectStr);
			if (replace_select_str === g_settingEngineURL) {
				g_SelectStr = g_settingEngineURL + g_SelectStr;
			} else {
				g_SelectStr = replace_select_str;
			}
		}
	}

	sendMessage(g_SelectStr, g_IsImage, g_IsBase64, g_IsAddressSearch);
}

// ドロップ
function handleDrop(e) {
	if ("INPUT" === e.target.nodeName.toString() || "TEXTAREA" === e.target.nodeName.toString() || true === e.shiftKey) {
		return;
	}

	eventInvalid(e);

	if ("" === g_SelectStr) {
		return;
	}

	if (true === g_IsImage) {
		// 画像の場合
		if (false === g_settingIsSaveImage) {
			return;
		}
		// Ctrlキーが押されている場合は画像をAPI使わずに保存
		if (true === g_IsBase64 || true === e.ctrlKey) {
			var anchor = document.createElement('a');
			anchor.href = g_SelectStr;
			anchor.download = '';
			anchor.style.display = 'none';
			document.body.appendChild(anchor);
			anchor.click();
			document.body.removeChild(anchor);
		} else {
			// Altキーが押されている場合は別タブに画像表示
			var message_type = 'downloadImage';
			if (true === e.altKey && false === e.ctrlKey) {
				message_type = 'searchURL';
			}
			// background.jsにメッセージを送信
			browser.runtime.sendMessage({
					type: message_type,
					value: g_SelectStr,
					isforground: true,
					tab: g_settingNewTabPosition,
				},
				// コールバック関数
				function (response) {
					if (response) {
						// response
					}
				});
		}
	} else {
		// タブを開く場合
		// Ctrlキーが押されている場合はクリップボードにコピー
		if (true === e.ctrlKey) {
			var clip = document.createElement("textarea");
			clip.value = e.dataTransfer.getData("text/plain");
			document.body.appendChild(clip);
			clip.select();
			document.execCommand("copy");
			clip.parentElement.removeChild(clip);
		}
		var message_type = 'searchURL';
		var isforground = true;
		if (g_IsAddressSearch) {
			// リンクでAltキーが押されている場合はダウンロード
			if (true === e.altKey && false === e.ctrlKey) {
				message_type = 'downloadImage';
			}
			isforground = g_settingIsAddressForground;
		} else {
			isforground = g_settingIsSearchForground;
		}
		// background.jsにメッセージを送信
		browser.runtime.sendMessage({
				type: message_type,
				value: g_SelectStr,
				isforground: isforground,
				tab: g_settingNewTabPosition,
			},
			// コールバック関数
			function (response) {
				if (response) {
					// response
				}
			});
	}
}


browser.storage.local.get(["searchEngineURL", "tabPosition", "checkboxArray"], function (storage_data) {
	if ('searchEngineURL' in storage_data) {
		updateParamEngine(storage_data.searchEngineURL);
	}

	if ('tabPosition' in storage_data) {
		updateNewTabPosition(storage_data.tabPosition);
	}

	if ('checkboxArray' in storage_data) {
		updateParamcheckboxArray(storage_data.checkboxArray);
	}
});
browser.storage.onChanged.addListener(function (storage_data_obj, area) {
	if (area == "local") {
		if ('searchEngineURL' in storage_data_obj) {
			updateParamEngine(storage_data_obj.searchEngineURL.newValue);
		}

		if ('tabPosition' in storage_data_obj) {
			updateNewTabPosition(storage_data_obj.tabPosition.newValue);
		}

		if ('checkboxArray' in storage_data_obj) {
			updateParamcheckboxArray(storage_data_obj.checkboxArray.newValue);
		}
	}
});
document.addEventListener("dragstart", handleDragStart, false);
document.addEventListener("dragover", eventInvalid, false);
document.addEventListener("dragend", eventInvalid, false);
document.addEventListener("drop", handleDrop, false);
window.addEventListener('message', receiveMessage, false);
var iframes = document.getElementsByTagName('iframe');
for (var i = 0; i < iframes.length; i++) {
	iframes[i].addEventListener('load', function() {
		iframes[i].addEventListener('message', receiveMessage, false);
	}, false);
}

var frames = document.getElementsByTagName('frame');
for (var i = 0; i < frames.length; i++) {
	frames[i].addEventListener('load', function() {
		frames[i].addEventListener('message', receiveMessage, false);
	}, false);
}