//パァ($・($・・)/~~~・)/~~~
'use strict';

browser.browserAction.onClicked.addListener(tab => {

	browser.sessions.getRecentlyClosed(sessionInfos => {
		
				for (let i = 0; i < sessionInfos.length; i++) {
			let sessionInfo = sessionInfos[i]; 
			
			if (sessionInfo.tab && sessionInfo.tab.windowId === tab.windowId) {
				
				if (sessionInfo.tab.sessionId != null) {
					chrome.sessions.restore(sessionInfo.tab.sessionId);
				}

				else{

				}
				
				break;
			}
		}
		
	})
});
