/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
function doImport()
{
  var lines =
    document.forms["foo"].elements["testList"].value.split(/\r?\n/);
  var suites = window.opener.suites;
  var elems = window.opener.document.forms["testCases"].elements;

  if (document.forms["foo"].elements["clear_all"].checked)
    window.opener._selectNone();

  for (var l in lines)
  {
    if (!lines[l])
    {
      continue;
    }

    if (lines[l].search(/^\s$|\s*\#/) == -1)
    {
      var ary = lines[l].match (/(.*)\/(.*)\/(.*)/);

      if (!ary) 
      {
        if (!confirm ("Line " + l + " '" +
                      lines[l] + "' is confusing, " +
                      "continue with import?"))
        {
          return;
        }
      }
                  
      if (suites[ary[1]] && 
          suites[ary[1]].testDirs[ary[2]] &&
          suites[ary[1]].testDirs[ary[2]].tests[ary[3]])
      {
        var radioname = suites[ary[1]].testDirs[ary[2]].tests[ary[3]].id;
        var radio = elems[radioname];
        if (!radio.checked)
        {
          radio.checked = true;  
          window.opener.onRadioClick(radio);
        }
      }
    }
  }

  setTimeout('window.close();', 200);
                  
}
