function AbNewCard()
{
	var dialog = window.openDialog("chrome://addressbook/content/newcardDialog.xul",
								   "abNewCard",
								   "chrome");
	return dialog;
}


function AbEditCard()
{
	var dialog = window.openDialog("chrome://addressbook/content/editcardDialog.xul",
								   "abEditCard",
								   "chrome");
	return dialog;
}

