function ResultsPaneSelectionChange()
{
	// not in ab window if no parent.parent.rdf
	if ( parent.parent.rdf )
	{
		var doc = parent.parent.frames["resultsFrame"].document;
		
		var selArray = doc.getElementsByAttribute('selected', 'true');
		if ( selArray && (selArray.length == 1) )
			DisplayCardViewPane(selArray[0]);
		else
			ClearCardViewPane();
	}
}

