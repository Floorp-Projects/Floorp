function AbNewCard()
{
	var dialog = window.openDialog("chrome://addressbook/content/newcardDialog.xul",
								   "abNewCard",
								   "chrome");
	return dialog;
}


function AbEditCard(card)
{
	var dialog = window.openDialog("chrome://addressbook/content/editcardDialog.xul",
								   "abEditCard",
								   "chrome",
								   {card:card});
	
	return dialog;
}

