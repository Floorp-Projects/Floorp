if (window.opener && window.opener.runNextTest)
{
  if (window.opener.reportCallBack)
  {
    window.opener.reportCallBack(window.opener.gWindow);
  }
  setTimeout('window.opener.runNextTest()', 250);
}
else
{
  gPageCompleted = true;
}


