<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<div id="cse" style="width: 100%;">Loading</div>
<script src="http://www.google.com/jsapi" type="text/javascript"></script>
<script type="text/javascript">
function parseQueryFromUrl () {
  var queryParamName = "q";
  var search = window.location.search.substr(1);
  var parts = search.split('&');
  for (var i = 0; i < parts.length; i++) {
    var keyvaluepair = parts[i].split('=');
    if (decodeURIComponent(keyvaluepair[0]) == queryParamName) {
      return decodeURIComponent(keyvaluepair[1].replace(/\+/g, ' '));
    }
  }
  return '';
}

google.load('search', '1', {language : 'en'});
google.setOnLoadCallback(function() {
  var customSearchOptions = {};
  var customSearchControl = new google.search.CustomSearchControl(
    '017013284162333743052:rvlazd1zehe', customSearchOptions);
  customSearchControl.setResultSetSize(google.search.Search.FILTERED_CSE_RESULTSET);
  var options = new google.search.DrawOptions();
  options.enableSearchResultsOnly();
  customSearchControl.draw('cse', options);
  var queryFromUrl = parseQueryFromUrl();
  if (queryFromUrl) {
    var searchBox = document.getElementById("search-box");
    searchBox.value = queryFromUrl;
    searchBox.focus();
    searchBox.blur();
    customSearchControl.execute(queryFromUrl);
  }
}, true);
</script>

<link rel="stylesheet" href="http://www.google.com/cse/style/look/default.css" type="text/css" />

<style type="text/css">
  #cse table, #cse tr, #cse td {
    border: none;
  }

  .gsc-above-wrapper-area, .gsc-result-info-container {
	border: none;
  }

  .gsc-table-cell-snippet-close {
    border-color: #a0d0fb;
  }

  .gsc-resultsHeader {
    display : none;
  }
  .gsc-control-cse {
    font-family: "Trebuchet MS", sans-serif;
    border-color: #F0F8FF;
    background: none;
  }
  .gsc-tabHeader.gsc-tabhInactive {
    border-color: #E9E9E9;
    background-color: none;
  }
  .gsc-tabHeader.gsc-tabhActive {
    border-top-color: #FF9900;
    border-left-color: #E9E9E9;
    border-right-color: #E9E9E9;
    background-color: #FFFFFF;
  }
  .gsc-tabsArea {
    border-color: #E9E9E9;
  }
  .gsc-webResult.gsc-result,
  .gsc-results .gsc-imageResult {
    border-color: #F0F8FF;
    background-color: #FFFFFF;
  }
  .gsc-webResult.gsc-result:hover,
  .gsc-webResult.gsc-result.gsc-promotion:hover,
  .gsc-imageResult:hover {
    border-color: #F0F8FF;
    background-color: #FFFFFF;
  }
  .gs-webResult.gs-result a.gs-title:link,
  .gs-webResult.gs-result a.gs-title:link b,
  .gs-imageResult a.gs-title:link,
  .gs-imageResult a.gs-title:link b {
    color: #000000;
  }
  .gs-webResult.gs-result a.gs-title:visited,
  .gs-webResult.gs-result a.gs-title:visited b,
  .gs-imageResult a.gs-title:visited,
  .gs-imageResult a.gs-title:visited b {
    color: #000000;
  }
  .gs-webResult.gs-result a.gs-title:hover,
  .gs-webResult.gs-result a.gs-title:hover b,
  .gs-imageResult a.gs-title:hover,
  .gs-imageResult a.gs-title:hover b {
    color: #000000;
  }
  .gs-webResult.gs-result a.gs-title:active,
  .gs-webResult.gs-result a.gs-title:active b,
  .gs-imageResult a.gs-title:active,
  .gs-imageResult a.gs-title:active b {
    color: #000000;
  }
  .gsc-cursor-page {
    color: #000000;
  }
  a.gsc-trailing-more-results:link {
    color: #000000;
  }
  .gs-webResult .gs-snippet,
  .gs-imageResult .gs-snippet,
  .gs-fileFormatType {
    color: #000000;
  }
  .gs-webResult div.gs-visibleUrl,
  .gs-imageResult div.gs-visibleUrl {
    color: #003595;
  }
  .gs-webResult div.gs-visibleUrl-short {
    color: #003595;
  }
  .gs-webResult div.gs-visibleUrl-short {
    display: none;
  }
  .gs-webResult div.gs-visibleUrl-long {
    display: block;
  }
  .gs-promotion div.gs-visibleUrl-short {
    display: none;
  }
  .gs-promotion div.gs-visibleUrl-long {
    display: block;
  }
  .gsc-cursor-box {
    border-color: #F0F8FF;
  }
  .gsc-results .gsc-cursor-box .gsc-cursor-page {
    border-color: #F0F8FF;
    background-color: #FFFFFF;
    color: #000000;
  }
  .gsc-results .gsc-cursor-box .gsc-cursor-current-page {
    border-color: #FF9900;
    background-color: #FFFFFF;
    color: #000000;
  }
  .gsc-webResult.gsc-result.gsc-promotion {
    border-color: #336699;
    background-color: #FFFFFF;
  }
  .gs-promotion a.gs-title:link,
  .gs-promotion a.gs-title:link *,
  .gs-promotion .gs-snippet a:link {
    color: #00ffff;
  }
  .gs-promotion a.gs-title:visited,
  .gs-promotion a.gs-title:visited *,
  .gs-promotion .gs-snippet a:visited {
    color: #0000CC;
  }
  .gs-promotion a.gs-title:hover,
  .gs-promotion a.gs-title:hover *,
  .gs-promotion .gs-snippet a:hover {
    color: #0000CC;
  }
  .gs-promotion a.gs-title:active,
  .gs-promotion a.gs-title:active *,
  .gs-promotion .gs-snippet a:active {
    color: #0000CC;
  }
  .gs-promotion .gs-snippet,
  .gs-promotion .gs-title .gs-promotion-title-right,
  .gs-promotion .gs-title .gs-promotion-title-right *  {
    color: #000000;
  }
  .gs-promotion .gs-visibleUrl,
  .gs-promotion .gs-visibleUrl-short {
    color: #008000;
  }


</style>
