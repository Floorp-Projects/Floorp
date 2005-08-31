if (window.opener && window.opener.runNextTest)
{
  setTimeout('window.opener.runNextTest()', 250);
}
else
{
  gPageCompleted = true;
}


