function OnLoad()
{
	dump("OnLoad()\n");

	if (window.arguments && window.arguments[0]) {
		dump(window.arguments[0] + "\n");
	}
	return true;
}
