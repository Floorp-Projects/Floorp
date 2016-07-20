/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */

requestLongerTimeout(2);

var {Promise: promise} = Cu.import("resource://gre/modules/Promise.jsm", {});

const gHttpTestRoot = "http://example.com/browser/dom/base/test/";

/**
 * Enable local telemetry recording for the duration of the tests.
 */
var gOldContentCanRecord = false;
add_task(function* test_initialize() {
  // Because canRecordExtended is a per-process variable, we need to make sure
  // that all of the pages load in the same content process. Limit the number
  // of content processes to at most 1 (or 0 if e10s is off entirely).
  yield SpecialPowers.pushPrefEnv({ set: [[ "dom.ipc.processCount", 1 ]] });

  gOldContentCanRecord = yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function () {
    let telemetry = Cc["@mozilla.org/base/telemetry;1"].getService(Ci.nsITelemetry);
    let old = telemetry.canRecordExtended;
    telemetry.canRecordExtended = true;
    return old;
  });
  info("canRecord for content: " + gOldContentCanRecord);
});

add_task(function* () {
  // Check that use counters are incremented by SVGs loaded directly in iframes.
  yield check_use_counter_iframe("file_use_counter_svg_getElementById.svg",
                                 "SVGSVGELEMENT_GETELEMENTBYID");
  yield check_use_counter_iframe("file_use_counter_svg_currentScale.svg",
                                 "SVGSVGELEMENT_CURRENTSCALE_getter");
  yield check_use_counter_iframe("file_use_counter_svg_currentScale.svg",
                                 "SVGSVGELEMENT_CURRENTSCALE_setter");

  // Check that use counters are incremented by SVGs loaded as images.
  // Note that SVG images are not permitted to execute script, so we can only
  // check for properties here.
  yield check_use_counter_img("file_use_counter_svg_getElementById.svg",
                              "PROPERTY_FILL");
  yield check_use_counter_img("file_use_counter_svg_currentScale.svg",
                              "PROPERTY_FILL");

  // Check that use counters are incremented by directly loading SVGs
  // that reference patterns defined in another SVG file.
  yield check_use_counter_direct("file_use_counter_svg_fill_pattern.svg",
                                 "PROPERTY_FILLOPACITY", /*xfail=*/true);

  // Check that use counters are incremented by directly loading SVGs
  // that reference patterns defined in the same file or in data: URLs.
  yield check_use_counter_direct("file_use_counter_svg_fill_pattern_internal.svg",
                                 "PROPERTY_FILLOPACITY");
  // data: URLs don't correctly propagate to their referring document yet.
  //yield check_use_counter_direct("file_use_counter_svg_fill_pattern_data.svg",
  //                               "PROPERTY_FILL_OPACITY");

  // Check that use counters are incremented by SVGs loaded as CSS images in
  // pages loaded in iframes.  Again, SVG images in CSS aren't permitted to
  // execute script, so we need to use properties here.
  yield check_use_counter_iframe("file_use_counter_svg_background.html",
                                 "PROPERTY_FILL");
  yield check_use_counter_iframe("file_use_counter_svg_list_style_image.html",
                                 "PROPERTY_FILL");

  // Check that even loads from the imglib cache update use counters.  The
  // background images should still be there, because we just loaded them
  // in the last set of tests.  But we won't get updated counts for the
  // document counters, because we won't be re-parsing the SVG documents.
  yield check_use_counter_iframe("file_use_counter_svg_background.html",
                                 "PROPERTY_FILL", false);
  yield check_use_counter_iframe("file_use_counter_svg_list_style_image.html",
                                 "PROPERTY_FILL", false);
});

add_task(function* () {
  yield ContentTask.spawn(gBrowser.selectedBrowser, { oldCanRecord: gOldContentCanRecord }, function (arg) {
    Cu.import("resource://gre/modules/PromiseUtils.jsm");
    yield new Promise(resolve => {
      let telemetry = Cc["@mozilla.org/base/telemetry;1"].getService(Ci.nsITelemetry);
      telemetry.canRecordExtended = arg.oldCanRecord;
      resolve();
    });
  });
});


function waitForDestroyedDocuments() {
  let deferred = promise.defer();
  SpecialPowers.exactGC(window, deferred.resolve);
  return deferred.promise;
}

function waitForPageLoad(browser) {
  return ContentTask.spawn(browser, null, function*() {
    Cu.import("resource://gre/modules/PromiseUtils.jsm");
    yield new Promise(resolve => {
      let listener = () => {
        removeEventListener("load", listener, true);
        resolve();
      }
      addEventListener("load", listener, true);
    });
  });
}

function grabHistogramsFromContent(browser, use_counter_middlefix) {
  return ContentTask.spawn(browser, { middlefix: use_counter_middlefix }, function* (arg) {
    let telemetry = Cc["@mozilla.org/base/telemetry;1"].getService(Ci.nsITelemetry);
    function snapshot_histogram(name) {
      return telemetry.getHistogramById(name).snapshot();
    }

    let histogram_page_name = "USE_COUNTER2_" + arg.middlefix + "_PAGE";
    let histogram_document_name = "USE_COUNTER2_" + arg.middlefix + "_DOCUMENT";
    let histogram_page = snapshot_histogram(histogram_page_name);
    let histogram_document = snapshot_histogram(histogram_document_name);
    let histogram_docs = snapshot_histogram("CONTENT_DOCUMENTS_DESTROYED");
    let histogram_toplevel_docs = snapshot_histogram("TOP_LEVEL_CONTENT_DOCUMENTS_DESTROYED");
    return [histogram_page.sum, histogram_document.sum,
            histogram_docs.sum, histogram_toplevel_docs.sum];
  });
}

var check_use_counter_iframe = Task.async(function* (file, use_counter_middlefix, check_documents=true) {
  info("checking " + file + " with histogram " + use_counter_middlefix);

  let newTab = gBrowser.addTab( "about:blank");
  gBrowser.selectedTab = newTab;
  newTab.linkedBrowser.stop();

  // Hold on to the current values of the telemetry histograms we're
  // interested in.
  let [histogram_page_before, histogram_document_before,
       histogram_docs_before, histogram_toplevel_docs_before] =
      yield grabHistogramsFromContent(gBrowser.selectedBrowser, use_counter_middlefix);

  gBrowser.selectedBrowser.loadURI(gHttpTestRoot + "file_use_counter_outer.html");
  yield waitForPageLoad(gBrowser.selectedBrowser);

  // Inject our desired file into the iframe of the newly-loaded page.
  yield ContentTask.spawn(gBrowser.selectedBrowser, { file: file }, function(opts) {
    Cu.import("resource://gre/modules/PromiseUtils.jsm");
    let deferred = PromiseUtils.defer();

    let wu = content.window.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);

    let iframe = content.document.getElementById('content');
    iframe.src = opts.file;
    let listener = (event) => {
      event.target.removeEventListener("load", listener, true);

      // We flush the main document first, then the iframe's document to
      // ensure any propagation that might happen from child->parent should
      // have already happened when counters are reported to telemetry.
      wu.forceUseCounterFlush(content.document);
      wu.forceUseCounterFlush(iframe.contentDocument);

      deferred.resolve();
    };
    iframe.addEventListener("load", listener, true);

    return deferred.promise;
  });
  
  // Tear down the page.
  gBrowser.removeTab(newTab);

  // The histograms only get recorded when the document actually gets
  // destroyed, which might not have happened yet due to GC/CC effects, etc.
  // Try to force document destruction.
  yield waitForDestroyedDocuments();

  // Grab histograms again and compare.
  let [histogram_page_after, histogram_document_after,
       histogram_docs_after, histogram_toplevel_docs_after] =
      yield grabHistogramsFromContent(gBrowser.selectedBrowser, use_counter_middlefix);

  is(histogram_page_after, histogram_page_before + 1,
     "page counts for " + use_counter_middlefix + " after are correct");
  ok(histogram_toplevel_docs_after >= histogram_toplevel_docs_before + 1,
     "top level document counts are correct");
  if (check_documents) {
    is(histogram_document_after, histogram_document_before + 1,
       "document counts for " + use_counter_middlefix + " after are correct");
  }
});

var check_use_counter_img = Task.async(function* (file, use_counter_middlefix) {
  info("checking " + file + " as image with histogram " + use_counter_middlefix);

  let newTab = gBrowser.addTab("about:blank");
  gBrowser.selectedTab = newTab;
  newTab.linkedBrowser.stop();

  // Hold on to the current values of the telemetry histograms we're
  // interested in.
  let [histogram_page_before, histogram_document_before,
       histogram_docs_before, histogram_toplevel_docs_before] =
      yield grabHistogramsFromContent(gBrowser.selectedBrowser, use_counter_middlefix);

  gBrowser.selectedBrowser.loadURI(gHttpTestRoot + "file_use_counter_outer.html");
  yield waitForPageLoad(gBrowser.selectedBrowser);

  // Inject our desired file into the img of the newly-loaded page.
  yield ContentTask.spawn(gBrowser.selectedBrowser, { file: file }, function(opts) {
    Cu.import("resource://gre/modules/PromiseUtils.jsm");
    let deferred = PromiseUtils.defer();

    let img = content.document.getElementById('display');
    img.src = opts.file;
    let listener = (event) => {
      img.removeEventListener("load", listener, true);

      // Flush for the image.  It matters what order we do these in, so that
      // the image can propagate its use counters to the document prior to the
      // document reporting its use counters.
      let wu = content.window.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
      wu.forceUseCounterFlush(img);

      // Flush for the main window.
      wu.forceUseCounterFlush(content.document);

      deferred.resolve();
    };
    img.addEventListener("load", listener, true);

    return deferred.promise;
  });
  
  // Tear down the page.
  gBrowser.removeTab(newTab);

  // The histograms only get recorded when the document actually gets
  // destroyed, which might not have happened yet due to GC/CC effects, etc.
  // Try to force document destruction.
  yield waitForDestroyedDocuments();

  // Grab histograms again and compare.
  let [histogram_page_after, histogram_document_after,
       histogram_docs_after, histogram_toplevel_docs_after] =
      yield grabHistogramsFromContent(gBrowser.selectedBrowser, use_counter_middlefix);
  is(histogram_page_after, histogram_page_before + 1,
     "page counts for " + use_counter_middlefix + " after are correct");
  is(histogram_document_after, histogram_document_before + 1,
     "document counts for " + use_counter_middlefix + " after are correct");
  ok(histogram_toplevel_docs_after >= histogram_toplevel_docs_before + 1,
     "top level document counts are correct");
  // 2 documents: one for the outer html page containing the <img> element, and
  // one for the SVG image itself.
  ok(histogram_docs_after >= histogram_docs_before + 2,
     "document counts are correct");
});

var check_use_counter_direct = Task.async(function* (file, use_counter_middlefix, xfail=false) {
  info("checking " + file + " with histogram " + use_counter_middlefix);

  let newTab = gBrowser.addTab( "about:blank");
  gBrowser.selectedTab = newTab;
  newTab.linkedBrowser.stop();

  // Hold on to the current values of the telemetry histograms we're
  // interested in.
  let [histogram_page_before, histogram_document_before,
       histogram_docs_before, histogram_toplevel_docs_before] =
      yield grabHistogramsFromContent(gBrowser.selectedBrowser, use_counter_middlefix);

  gBrowser.selectedBrowser.loadURI(gHttpTestRoot + file);
  yield ContentTask.spawn(gBrowser.selectedBrowser, null, function*() {
    Cu.import("resource://gre/modules/PromiseUtils.jsm");
    yield new Promise(resolve => {
      let listener = () => {
        removeEventListener("load", listener, true);

        let wu = content.window.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
        wu.forceUseCounterFlush(content.document);

        setTimeout(resolve, 0);
      }
      addEventListener("load", listener, true);
    });
  });
  
  // Tear down the page.
  gBrowser.removeTab(newTab);

  // The histograms only get recorded when the document actually gets
  // destroyed, which might not have happened yet due to GC/CC effects, etc.
  // Try to force document destruction.
  yield waitForDestroyedDocuments();

  // Grab histograms again and compare.
  let [histogram_page_after, histogram_document_after,
       histogram_docs_after, histogram_toplevel_docs_after] =
      yield grabHistogramsFromContent(gBrowser.selectedBrowser, use_counter_middlefix);
  (xfail ? todo_is : is)(histogram_page_after, histogram_page_before + 1,
                         "page counts for " + use_counter_middlefix + " after are correct");
  (xfail ? todo_is : is)(histogram_document_after, histogram_document_before + 1,
                         "document counts for " + use_counter_middlefix + " after are correct");
  ok(histogram_toplevel_docs_after >= histogram_toplevel_docs_before + 1,
     "top level document counts are correct");
  ok(histogram_docs_after >= histogram_docs_before + 1,
     "document counts are correct");
});
