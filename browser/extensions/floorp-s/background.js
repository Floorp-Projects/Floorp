// background.js
// quickdrag.jsからメッセージを受信
browser.runtime.onMessage.addListener(
	function (request, sender, sendResponse) {
		switch (request.type) {
		case 'searchURL':
			searchURL(request, sender, sendResponse);
			break;
		case 'downloadImage':
			downloadImage(request, sender, sendResponse);
			break;
		default:
			// console.log("unknown type");
			// console.log(request);
			break;
		}
	}
);

// アクティブなタブの情報を取得
function getActiveTabInfo(tabs) {
	for (var tab of tabs) {
		if (tab.active) {
			return { openIndex: tab.index,
				 openId: tab.id
			};
		}
	}
}

// タブを開く
function searchURL(request, sender, callback) {
	browser.tabs.query({
		currentWindow: true
	}, function (tabs) {
		switch (request.tab) {
		case 'right':
			var { openIndex, openId } = getActiveTabInfo(tabs);
			openIndex += + 1;
			browser.tabs.create({
				url: request.value,
				cookieStoreId: sender.tab.cookieStoreId,
				active: request.isforground,
				index: openIndex,
				openerTabId: openId
			});
			break;
		case 'left':
			var { openIndex, openId } = getActiveTabInfo(tabs);
			browser.tabs.create({
				url: request.value,
				cookieStoreId: sender.tab.cookieStoreId,
				active: request.isforground,
				index: openIndex,
				openerTabId: openId
			});
			break;
		case 'last':
			browser.tabs.create({
				url: request.value,
				cookieStoreId: sender.tab.cookieStoreId,
				active: request.isforground
			});
			break;
		case 'first':
			var openIndex = 0;
			browser.tabs.create({
				url: request.value,
				cookieStoreId: sender.tab.cookieStoreId,
				active: request.isforground,
				index: openIndex
			});
			break;
		default:
			browser.tabs.create({
				url: request.value,
				cookieStoreId: sender.tab.cookieStoreId,
				active: request.isforground
			});
			break;
		}
		callback("searchURL:" + request.value);
	});
}

// 画像を保存
function downloadImage(request, sender, callback) {
	var downloading = browser.downloads.download({
		url: request.value,
		saveAs: true,
		conflictAction: "overwrite"
	});
	callback("downloadImage:" + downloading);
}