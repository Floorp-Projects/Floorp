function jsTestDriverEnd()
{
  // gDelayTestDriverEnd is used to
  // delay collection of the test result and 
  // signal to Spider so that tests can continue
  // to run after page load has fired. They are
  // responsible for setting gDelayTestDriverEnd = true
  // then when completed, setting gDelayTestDriverEnd = false
  // then calling jsTestDriverEnd()

  if (gDelayTestDriverEnd)
  {
    return;
  }

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
}

jsTestDriverEnd();


