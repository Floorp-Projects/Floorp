/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run(jQuery) {

  function showThirdPartyModules() {
    if ($("#third-party-module-summaries").html() != "") {
      $("#third-party-module-subsection").show();
    }
  }

  function highlightCode() {
    $("code").parent("pre").addClass("brush: js");
    //remove the inner <code> tags
    $('pre>code').each(function() {
      var inner = $(this).contents();
      $(this).replaceWith(inner);
    })
    SyntaxHighlighter.highlight();
  }

  function generateAnchors() {
    var headings = '#main-content h2, #main-content h3, ' +
                   '#main-content h4, #main-content h5, ' +
                   '#main-content h6';

    if ($(headings).length == 0) {
      $("#toc").hide();
      return;
    }

    var suffix = 1;
    var headingIDs = new Array();

    $(headings).each(function(i) {
      var baseName = $(this).text();
      // Remove the datatype portion of properties
      var dataTypeStart = baseName.indexOf(" : ");
      if (dataTypeStart != -1)
        baseName = baseName.slice(0, dataTypeStart);
      // Uniqueify the name of the heading
      var suffixedName = baseName;
      var headingIDExists = headingIDs.indexOf(suffixedName) != -1;
      while (headingIDExists) {
        suffix++;
        suffixedName = baseName + "_" + suffix;
        headingIDExists = headingIDs.indexOf(suffixedName) != -1;
      }
      headingIDs.push(suffixedName);
      // Now add the ID attribute
      $(this).attr("id", suffixedName);
   });
  }

  function stripArgumentsOrDataType(apiName) {
    var argumentStart = apiName.indexOf("(");
    if (argumentStart != -1) {
        return apiName.slice(0, argumentStart) + "()";
    }
    var dataTypeStart = apiName.indexOf(":");
    if (dataTypeStart != -1) {
        return apiName.slice(0, dataTypeStart);
    }
  return apiName;
  }

  function generateToC() {
    var headings = '.api_reference h2, .api_reference h3, ' +
                   '.api_reference h4, .api_reference h5, ' +
                   '.api_reference h6';

    if ($(headings).length == 0) {
      $("#toc").hide();
      return;
    }

    var pageURL = document.location.protocol + "//" +
                  document.location.host +
                  document.location.pathname +
                  document.location.search;

    $(headings).each(function(i) {
      var url = pageURL + "#" + encodeURIComponent($(this).attr("id"));
      var tocEntry = $("<a></a>").attr({
        href: url,
        "class": $(this).attr("tagName")
      });
      tocEntry.text(stripArgumentsOrDataType($(this).text()));
      $("#toc").append(tocEntry);
    });

  }

  function jumpToAnchor() {
    // Make Firefox jump to the anchor even though we created it after it
    // parsed the page.
    var pageURL = document.location.protocol + "//" +
                  document.location.host +
                  document.location.pathname +
                  document.location.search;
    if (document.location.hash) {
      var hash = document.location.hash;
      document.location.replace(pageURL + "#");
      document.location.replace(pageURL + hash);
    }
  }

  function refreshSearchBox() {
    var searchBox = document.getElementById("search-box");
    searchBox.value = "";
    searchBox.focus();
    searchBox.blur();
  }

  showThirdPartyModules();
  highlightCode();
  $(".syntaxhighlighter").width("auto");
  generateAnchors();
  generateToC();
  refreshSearchBox();
  jumpToAnchor();
}

$(window).ready(function() {
  run(jQuery);
});
