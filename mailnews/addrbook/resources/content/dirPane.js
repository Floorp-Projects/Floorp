function ChangeDirectoryByDOMNode(dirNode)
{
	var uri = dirNode.getAttribute('id');
	dump(uri + "\n");
	ChangeDirectoryByURI(uri);
}

function ChangeDirectoryByURI(uri)
{
	var tree = top.window.frames[0].frames[1].document.getElementById('resultTree');
	//dump("tree = " + tree + "\n");
        tree.setAttribute('ref', uri);// body no longer valid after setting id.
}
