				function restoreMostRecent(session) {
					if (!session.length) {
					  return;
					}
					let sessionInfo = session[0];
					if (sessionInfo.tab) {
					  browser.sessions.restore(sessionInfo.tab.sessionId);
					} else {
					  browser.sessions.restore(sessionInfo.window.sessionId);
					}
				  }

				  browser.browserAction.onClicked.addListener(function() {
					let gettingSessions = browser.sessions.getRecentlyClosed({
					  maxResults: 1
					});
					gettingSessions.then(restoreMostRecent);
				  });
