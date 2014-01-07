// ----------------------
// DumpDOM(node)
//
// Call this function to dump the contents of the DOM starting at the specified node.
// Use node = document.documentElement to dump every element of the current document.
// Use node = top.window.document.documentElement to dump every element.
//
// 8-13-99 Updated to dump almost all attributes of every node.  There are still some attributes
//         that are purposely skipped to make it more readable.
// ----------------------
function DumpDOM(node)
{
	dump("--------------------- DumpDOM ---------------------\n");
	
	DumpNodeAndChildren(node, "");
	
	dump("------------------- End DumpDOM -------------------\n");
}


// This function does the work of DumpDOM by recursively calling itself to explore the tree
function DumpNodeAndChildren(node, prefix)
{
	dump(prefix + "<" + node.nodeName);

	var attributes = node.attributes;
	
	if ( attributes && attributes.length )
	{
		var item, name, value;
		
		for ( var index = 0; index < attributes.length; index++ )
		{
			item = attributes.item(index);
			name = item.nodeName;
			value = item.nodeValue;
			
			if ( (name == 'lazycontent' && value == 'true') ||
				 (name == 'xulcontentsgenerated' && value == 'true') ||
				 (name == 'id') ||
				 (name == 'instanceOf') )
			{
				// ignore these
			}
			else
			{
				dump(" " + name + "=\"" + value + "\"");
			}
		}
	}
	
	if ( node.nodeType == 1 )
	{
		// id
		var text = node.getAttribute('id');
		if ( text && text[0] != '$' )
			dump(" id=\"" + text + "\"");
	}
	
	if ( node.nodeType == Node.TEXT_NODE )
		dump(" = \"" + node.data + "\"");
	
	dump(">\n");
	
	// dump IFRAME && FRAME DOM
	if ( node.nodeName == "IFRAME" || node.nodeName == "FRAME" )
	{
		if ( node.name )
		{
			var wind = top.frames[node.name];
			if ( wind && wind.document && wind.document.documentElement )
			{
				dump(prefix + "----------- " + node.nodeName + " -----------\n");
				DumpNodeAndChildren(wind.document.documentElement, prefix + "  ");
				dump(prefix + "--------- End " + node.nodeName + " ---------\n");
			}
		}
	}
	// children of nodes (other than frames)
	else if ( node.childNodes )
	{
		for ( var child = 0; child < node.childNodes.length; child++ )
			DumpNodeAndChildren(node.childNodes[child], prefix + "  ");
	} 
}
