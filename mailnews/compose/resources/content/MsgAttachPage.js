var result = null;

function Startup()
{
	/* dump("Startup()\n"); */

	if (window.arguments && window.arguments[0] && window.arguments[0])
    {
		result = window.arguments[0];
		doSetOKCancel(AttachPageOKCallback, AttachPageCancelCallback);
		moveToAlertPosition();
	}
	else 
	{
		dump("error, no return object registered\n");
	}

}

function AttachPageOKCallback()
{
	/* dump("attach this: " + document.getElementById('attachurl').value + "\n"); */

    result.url = document.getElementById('attachurl').value;
    return true;
}
            
function AttachPageCancelCallback()
{
	return true;
}
