var addattachmentfunction = null;

function Startup()
{
	/* dump("Startup()\n"); */

	if (window.arguments && window.arguments[0] && window.arguments[0].addattachmentfunction)
        {
		addattachmentfunction = window.arguments[0].addattachmentfunction;
		doSetOKCancel(AttachPageOKCallback, AttachPageCancelCallback);
	}
	else 
	{
		dump("error, no callback registered for OK\n");
	}

}

function AttachPageOKCallback()
{
	/* dump("attach this: " + document.getElementById('attachurl').value + "\n"); */

	if (addattachmentfunction) {
		addattachmentfunction(document.getElementById('attachurl').value);
	}
                                                                                               return true;
}
            
function AttachPageCancelCallback()
{
	return true;
}
