var gSubscribetree;
var gServername;

function SubscribeOnLoad()
{
	dump("subscribeOnLoad()\n");

    gSubscribetree = document.getElementById('subscribetree');
    gServername = document.getElementById('servername');

	gSubscribetree.setAttribute('ref','news://news.mozilla.org');
	gServername.value = "news.mozilla.org";
}

function SubscribeButtonClicked()
{
	dump("subscribe button clicked\n");
}

function UnsubscribeButtonClicked()
{
	dump("unsubscribe button clicked\n");
}

function SubscribeOnClick(event)
{
	dump("subscribe tree clicked\n");
	dump(event + "\n");
}
