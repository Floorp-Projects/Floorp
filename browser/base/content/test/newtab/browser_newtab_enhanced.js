/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

requestLongerTimeout(2);

const PRELOAD_PREF = "browser.newtab.preload";

var suggestedLink = {
  url: "http://example1.com/2",
  imageURI: "data:image/png;base64,helloWORLD3",
  title: "title2",
  type: "affiliate",
  adgroup_name: "Technology",
  frecent_sites: ["classroom.google.com", "codeacademy.org", "codecademy.com", "codeschool.com", "codeyear.com", "elearning.ut.ac.id", "how-to-build-websites.com", "htmlcodetutorial.com", "htmldog.com", "htmlplayground.com", "learn.jquery.com", "quackit.com", "roseindia.net", "teamtreehouse.com", "tizag.com", "tutorialspoint.com", "udacity.com", "w3schools.com", "webdevelopersnotes.com"]
};

gDirectorySource = "data:application/json," + JSON.stringify({
  "enhanced": [{
    url: "http://example.com/",
    enhancedImageURI: "data:image/png;base64,helloWORLD",
    title: "title",
    type: "organic",
  }],
  "directory": [{
    url: "http://example1.com/",
    enhancedImageURI: "data:image/png;base64,helloWORLD2",
    title: "title1",
    type: "organic"
  }],
  "suggested": [suggestedLink]
});

add_task(function* () {
  let origEnhanced = NewTabUtils.allPages.enhanced;
  registerCleanupFunction(() => {
    NewTabUtils.allPages.enhanced = origEnhanced;
  });

  yield pushPrefs([PRELOAD_PREF, false]);

  function getData(cellNum) {
    return performOnCell(cellNum, cell => {
      if (!cell.site)
        return null;
      let siteNode = cell.site.node;
      return {
        type: siteNode.getAttribute("type"),
        enhanced: siteNode.querySelector(".enhanced-content").style.backgroundImage,
        title: siteNode.querySelector(".newtab-title").textContent,
        suggested: siteNode.querySelector(".newtab-suggested").innerHTML
      };
    });
  }

  // Make the page have a directory link, enhanced link, and history link
  yield setLinks("-1");

  // Test with enhanced = false
  yield* addNewTabPageTab();
  yield customizeNewTabPage("classic");
  yield customizeNewTabPage("enhanced"); // Toggle enhanced off
  let {type, enhanced, title, suggested} = yield getData(0);
  isnot(type, "enhanced", "history link is not enhanced");
  is(enhanced, "", "history link has no enhanced image");
  is(title, "example.com");
  is(suggested, "", "There is no suggested explanation");

  let data = yield getData(1);
  is(data, null, "there is only one link and it's a history link");

  // Test with enhanced = true
  yield* addNewTabPageTab();
  yield customizeNewTabPage("enhanced"); // Toggle enhanced on
  ({type, enhanced, title, suggested} = yield getData(0));
  is(type, "organic", "directory link is organic");
  isnot(enhanced, "", "directory link has enhanced image");
  is(title, "title1");
  is(suggested, "", "There is no suggested explanation");

  ({type, enhanced, title, suggested} = yield getData(1));
  is(type, "enhanced", "history link is enhanced");
  isnot(enhanced, "", "history link has enhanced image");
  is(title, "title");
  is(suggested, "", "There is no suggested explanation");

  data = yield getData(2);
  is(data, null, "there are only 2 links, directory and enhanced history");

  // Test with a pinned link
  setPinnedLinks("-1");
  yield* addNewTabPageTab();
  ({type, enhanced, title, suggested} = yield getData(0));
  is(type, "enhanced", "pinned history link is enhanced");
  isnot(enhanced, "", "pinned history link has enhanced image");
  is(title, "title");
  is(suggested, "", "There is no suggested explanation");

  ({type, enhanced, title, suggested} = yield getData(1));
  is(type, "organic", "directory link is organic");
  isnot(enhanced, "", "directory link has enhanced image");
  is(title, "title1");
  is(suggested, "", "There is no suggested explanation");

  data = yield getData(2);
  is(data, null, "directory link pushed out by pinned history link");

  // Test pinned link with enhanced = false
  yield* addNewTabPageTab();
  yield customizeNewTabPage("enhanced"); // Toggle enhanced off
  ({type, enhanced, title, suggested} = yield getData(0));
  isnot(type, "enhanced", "history link is not enhanced");
  is(enhanced, "", "history link has no enhanced image");
  is(title, "example.com");
  is(suggested, "", "There is no suggested explanation");

  data = yield getData(1);
  is(data, null, "directory link still pushed out by pinned history link");

  yield unpinCell(0);



  // Test that a suggested tile is not enhanced by a directory tile
  NewTabUtils.isTopPlacesSite = () => true;
  yield setLinks("-1,2,3,4,5,6,7,8");

  // Test with enhanced = false
  yield* addNewTabPageTab();
  ({type, enhanced, title, suggested} = yield getData(0));
  isnot(type, "enhanced", "history link is not enhanced");
  is(enhanced, "", "history link has no enhanced image");
  is(title, "example.com");
  is(suggested, "", "There is no suggested explanation");

  data = yield getData(7);
  isnot(data, null, "there are 8 history links");
  data = yield getData(8);
  is(data, null, "there are 8 history links");


  // Test with enhanced = true
  yield* addNewTabPageTab();
  yield customizeNewTabPage("enhanced");

  // Suggested link was not enhanced by directory link with same domain
  ({type, enhanced, title, suggested} = yield getData(0));
  is(type, "affiliate", "suggested link is affiliate");
  is(enhanced, "", "suggested link has no enhanced image");
  is(title, "title2");
  ok(suggested.indexOf("Suggested for <strong> Technology </strong> visitors") > -1, "Suggested for 'Technology'");

  // Enhanced history link shows up second
  ({type, enhanced, title, suggested} = yield getData(1));
  is(type, "enhanced", "pinned history link is enhanced");
  isnot(enhanced, "", "pinned history link has enhanced image");
  is(title, "title");
  is(suggested, "", "There is no suggested explanation");

  data = yield getData(9);
  is(data, null, "there is a suggested link followed by an enhanced history link and the remaining history links");



  // Test no override category/adgroup name.
  let linksChangedPromise = watchLinksChangeOnce();
  yield pushPrefs([PREF_NEWTAB_DIRECTORYSOURCE,
                  "data:application/json," + JSON.stringify({"suggested": [suggestedLink]})]);
  yield linksChangedPromise;

  yield* addNewTabPageTab();
  ({type, enhanced, title, suggested} = yield getData(0));
  Cu.reportError("SUGGEST " + suggested);
  ok(suggested.indexOf("Suggested for <strong> Technology </strong> visitors") > -1, "Suggested for 'Technology'");


  // Test server provided explanation string.
  suggestedLink.explanation = "Suggested for %1$S enthusiasts who visit sites like %2$S";
  linksChangedPromise = watchLinksChangeOnce();
  yield pushPrefs([PREF_NEWTAB_DIRECTORYSOURCE,
                  "data:application/json," + encodeURIComponent(JSON.stringify({"suggested": [suggestedLink]}))]);
  yield linksChangedPromise;

  yield* addNewTabPageTab();
  ({type, enhanced, title, suggested} = yield getData(0));
  Cu.reportError("SUGGEST " + suggested);
  ok(suggested.indexOf("Suggested for <strong> Technology </strong> enthusiasts who visit sites like <strong> classroom.google.com </strong>") > -1, "Suggested for 'Technology' enthusiasts");


  // Test server provided explanation string with category override.
  suggestedLink.adgroup_name = "webdev education";
  linksChangedPromise = watchLinksChangeOnce();
  yield pushPrefs([PREF_NEWTAB_DIRECTORYSOURCE,
                  "data:application/json," + encodeURIComponent(JSON.stringify({"suggested": [suggestedLink]}))]);
  yield linksChangedPromise;

  yield* addNewTabPageTab();
  ({type, enhanced, title, suggested} = yield getData(0));
  Cu.reportError("SUGGEST " + suggested);
  ok(suggested.indexOf("Suggested for <strong> webdev education </strong> enthusiasts who visit sites like <strong> classroom.google.com </strong>") > -1, "Suggested for 'webdev education' enthusiasts");



  // Test with xml entities in category name
  suggestedLink.url = "http://example1.com/3";
  suggestedLink.adgroup_name = ">angles< & \"quotes\'";
  linksChangedPromise = watchLinksChangeOnce();
  yield pushPrefs([PREF_NEWTAB_DIRECTORYSOURCE,
                  "data:application/json," + encodeURIComponent(JSON.stringify({"suggested": [suggestedLink]}))]);
  yield linksChangedPromise;

  yield* addNewTabPageTab();
  ({type, enhanced, title, suggested} = yield getData(0));
  Cu.reportError("SUGGEST " + suggested);
  ok(suggested.indexOf("Suggested for <strong> &gt;angles&lt; &amp; \"quotes\' </strong> enthusiasts who visit sites like <strong> classroom.google.com </strong>") > -1, "Suggested for 'xml entities' enthusiasts");


  // Test with xml entities in explanation.
  suggestedLink.explanation = "Testing junk explanation &<>\"'";
  linksChangedPromise = watchLinksChangeOnce();
  yield pushPrefs([PREF_NEWTAB_DIRECTORYSOURCE,
                  "data:application/json," + encodeURIComponent(JSON.stringify({"suggested": [suggestedLink]}))]);
  yield linksChangedPromise;

  yield* addNewTabPageTab();
  ({type, enhanced, title, suggested} = yield getData(0));
  Cu.reportError("SUGGEST " + suggested);
  ok(suggested.indexOf("Testing junk explanation &amp;&lt;&gt;\"'") > -1, "Junk test");
});
