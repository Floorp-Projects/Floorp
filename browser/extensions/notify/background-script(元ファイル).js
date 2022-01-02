/*
Log that we received the message.
Then display a notification. The notification contains the URL,
which we read from the message.
*/ 
var link;

function init() {
  var gettingInfo = browser.runtime.getBrowserInfo();
  gettingInfo.then(notify); 
}

async function notify(info) {
  var newvar;
  await fetch( 'https://repo.ablaze.one/api/?name=floorp', { method: 'GET' })
  .then( response => response.json() )
  .then( jsObject =>{ 
newvar = jsObject.build
link =jsObject.file
})
  .catch( error => {return error;});
  if (newvar==info.version){
    return;
  }
  else if(newvar==undefined) {
    return;
  }
  else {
  var ni = [info.version, newvar];
  var title = browser.i18n.getMessage("notificationTitle");
  var content = browser.i18n.getMessage("notificationContent", ni);
  browser.notifications.create({
    "type": "basic",
    "iconUrl": browser.runtime.getURL("icons/link-48.png"),
    "title": title,
    "message": content
  });
  browser.notifications.onClicked.addListener(open);
  }
}
function open() {
  browser.tabs.create({
   "url": link
  });
}
/*
Assign `notify()` as a listener to messages from the content script.
*/
window.addEventListener('load',init);