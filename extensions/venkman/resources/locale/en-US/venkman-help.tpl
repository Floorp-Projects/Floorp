<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0//EN">

<html>
  <head>
    <meta http-equiv="content-type" content="text/html; charset=$report-charset">
    <link rel="stylesheet" href="$css" type="text/css" media="screen">

    <title>Venkman Help System</title>
    <script>
      function onLoad ()
      {
          var searchStart = document.location.href.indexOf("?");
          if (searchStart != -1)
          {
              var href = document.location.href;
              var result = parseSearch(href.substr(searchStart + 1));

              if ("search" in result)
              {
                  document.getElementById("search").value =
                      decodeURIComponent(result.search);
              }

              if ("within" in result)
              {
                  var within = parseInt(result.within);
                  if (within & 0x01)
                      document.getElementById("command-names").checked = true;
                  if (within & 0x02)
                      document.getElementById("ui-labels").checked = true;
                  if (within & 0x04)
                      document.getElementById("help-text").checked = true;
              }
              else
              {
                  document.getElementById("command-names").checked = true;
                  document.getElementById("ui-labels").checked = true;
              }
          }
          else
          {
              document.getElementById("command-names").checked = true;
              document.getElementById("ui-labels").checked = true;
          }
      }

      function onKeyUp (event)
      {
          if (event.keyCode == 13)
              onSearch();
      }

      function onSearch ()
      {
          var search = document.getElementById("search").value;

          var within = 0;
          if (document.getElementById("command-names").checked)
              within |= 0x01;
          if (document.getElementById("ui-labels").checked)
              within |= 0x02;
          if (document.getElementById("help-text").checked)
              within |= 0x04;

          document.location.href = "x-jsd:help?search=" + escape(search) +
                                   "&within=" + within;
      }

      function parseSearch (search)
      {
          var parseResult = new Object();
          var ary = search.match(/([^&]+)/);
          
          while (ary)
          {
              var rest = RegExp.rightContext.substr(1);
              var assignment = ary[1];
              ary = assignment.match(/(.+)=(.*)/);
              if (ary)
              {
                  /* only set the property the first time we see it */
                  if (2 in ary && !(ary[1] in parseResult))
                      parseResult[ary[1]] = ary[2];
              }
              ary = rest.match(/([^&]+)/);
          }

          return parseResult;
      }
    </script>
  </head>

  <body id="venkman-help" onload="onLoad();"
    hasSearched="$has-searched" matchCount="$match-count">

    <a name="top"></a>
    <div id="help-menu">
      <span class="newbie-help">
        <p>
          Welcome to the <b>Venkman Help System</b>.  From here you can search
          for help on the various commands available in Venkman.  To search for a
          particular command, type your search term in the box below and click
          <b>Go</b>, or press <b>Enter</b>.
        <p>
          Additional help can be found on the
          <a href="http://www.mozilla.org/projects/venkman/">Venkman Homepage</a>
          or the
          <a href="http://www.hacksrus.com/~ginda/venkman/faq/venkman-faq.html">Venkman FAQ</a>.
      </span>
      <span class="newbie-help">
        The check boxes control which fields the search is performed on...
        <ul>
          <li><b>Command Names</b> matches the command name, as you might enter
            it in the </b>Interactive Session</b>.
          <li><b>User Interface Labels</b> matches the label used when the
            command appears in a <b>menu</b> or <b>toolbar button</b>.
          <li><b>Descriptions</b> matches the body of the help text.
        </ul>
      </span>
      <p class="search-input">
        Search: <input type="text" id="search" onkeyup="onKeyUp(event)"/>
        <input type="button" onclick="onSearch()" value="Go">

        <input type="checkbox" id="command-names" value="1">
        <label for="command-names">Command Names</label>

        <input type="checkbox" id="ui-labels" value="1">
        <label for="ui-labels">User Interface Labels</label>

        <input type="checkbox" id="help-text" value="1">
        <label for="help-text">Descriptions</label>
      <p class="quick-searches">
        [ <a href="x-jsd:help">Interactive Session Commands</a> |
         <a href="x-jsd:help?search="><b>All</b> Commands</a> |
         <a href="x-jsd:help?search=%5E%5Ba-c%5D&within=2">A-C</a> |
         <a href="x-jsd:help?search=%5E%5Bd-f%5D&within=2">D-F</a> |
         <a href="x-jsd:help?search=%5E%5Bg-i%5D&within=2">G-I</a> |
         <a href="x-jsd:help?search=%5E%5Bj-l%5D&within=2">J-L</a> |
         <a href="x-jsd:help?search=%5E%5Bm-o%5D&within=2">M-O</a> |
         <a href="x-jsd:help?search=%5E%5Bp-r%5D&within=2">P-R</a> |
         <a href="x-jsd:help?search=%5E%5Bs-u%5D&within=2">S-U</a> |
         <a href="x-jsd:help?search=%5E%5Bv-z%5D&within=2">V-Z</a> ]
    </div>         
       
    <span id="match-count">Found $match-count matching command(s).</span>

    <span id="command-list">
@-header-end
      <span class="command">
        <span class="label" item="command-name">Command Name:</span>
        <span class="value" item="command-name"><a href="x-jsd:help?search=$command-name">$command-name</a></span><br>
        <span class="label" item="ui-label">User Interface Label:</span>
        <span class="value" item="ui-label"><a href="x-jsd:help?search=$ui-label-safe&within=2">$ui-label</a></span><br>
        <br>
        <span class="label" item="usage">Usage:</span> <span class="value" item="usage">$command-name $params</span><br>
        <br>
        <span class="label" item="accel-key">Accelerator Key:</span> $key<br>
        <br>
        <span class="label" item="description">Description:</span><br>
        <span class="value" item="description">$desc</span>
        <span class="goto-top"><a href="#top">Back To Top</a></span>
      </span>
      <hr>
@-command-end
      <font color="red"><b>No commands found</b></font>
@-nomatch-end
    </span>
  </body>
</html>
