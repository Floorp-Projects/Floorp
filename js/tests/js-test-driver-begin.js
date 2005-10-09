/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
var gVersion = 150;
var gTestName = '';
var gTestPath = '';

function init()
{
  if (document.location.search.indexOf('?') != 0)
  {
    // not called with a query string
    return;
  }

  var re = /test=([^;]+);language=(language|type);([a-zA-Z0-9.=;\/]+)/;
  var matches = re.exec(document.location.search);

  // testpath http://machine/path-to-suite/sub-suite/test.js
  var testpath  = matches[1];
  var attribute = matches[2];
  var value     = matches[3];

  if (testpath)
  {
    gTestPath = testpath;
  }

  var ise4x = /e4x\//.test(testpath);

  if (value.indexOf('1.1') != -1)
  {
    gVersion = 110;
  }
  else if (value.indexOf('1.2') != -1)
  {
    gVersion = 120;
  }
  else if (value.indexOf('1.3') != -1)
  {
    gVersion = 130;
  }
  else if (value.indexOf('1.4') != -1)
  {
    gVersion = 140;
  }
  else if(value.indexOf('1.5') != -1)
  {
    gVersion = 150;
  }

  var testpathparts = testpath.split(/\//);

  if (testpathparts.length < 3)
  {
    // must have at least suitepath/subsuite/testcase.js
    return;
  }
  var suitepath = testpathparts.slice(0,testpathparts.length-2).join('/');
  var subsuite  = testpathparts[testpathparts.length - 2];
  var test      = testpathparts[testpathparts.length - 1];

  gTestName = test;

  outputscripttag(suitepath + '/shell.js', attribute, value, ise4x);
  outputscripttag(suitepath + '/browser.js', attribute, value, ise4x);
  outputscripttag(suitepath + '/' + subsuite + '/shell.js', attribute, value, ise4x);
  outputscripttag(suitepath + '/' + subsuite + '/browser.js', attribute, value, ise4x);
  outputscripttag(suitepath + '/' + subsuite + '/' + test, attribute, value, ise4x);

  document.write('<title>' + suitepath + '/' + subsuite + '/' + test + '<\/title>');
  return;
}

function outputscripttag(src, attribute, value, ise4x)
{
  if (!src)
  {
    return;
  }

  var s = '<script src="' +  src + '" ';

  if (ise4x)
  {
    if (attribute == 'type')
    {
      value += ';e4x=1 ';
    }
    else
    {
      s += ' type="text/javascript';
      if (gVersion != 150)
      {
        s += ';version=' + gVersion/100;
      }
      s += ';e4x=1" ';
    }
  }

  s +=  attribute + '="' + value + '"><\/script>';

  document.write(s);
}

init();

