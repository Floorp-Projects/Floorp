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

	var treechildrenList = tree.getElementsByTagName('treechildren');
	if ( treechildrenList.length == 1 )
	{
		var body = treechildrenList[0];
		body.setAttribute('id', uri);// body no longer valid after setting id.
	}
}
