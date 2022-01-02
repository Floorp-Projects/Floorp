/**
 * TestRunner: A test runner for SimpleTest
 * TODO:
 * 
 *  * Avoid moving iframes: That causes reloads on mozilla and opera.
 *
 *
**/
var TestRunner = {};
TestRunner.logEnabled = false;
TestRunner._iframes = {};
TestRunner._iframeDocuments = {};
TestRunner._iframeRows = {};
TestRunner._currentTest = 0;
TestRunner._urls = [];
TestRunner._testsDiv = DIV();
TestRunner._progressDiv = DIV();
TestRunner._summaryDiv = DIV(null, 
    H1(null, "Tests Summary"),
    TABLE(null, 
        THEAD(null, 
            TR(null,
                TH(null, "Test"), 
                TH(null, "Passed"), 
                TH(null, "Failed")
            )
        ),
        TBODY()
    )
);

/**
 * This function is called after generating the summary.
**/
TestRunner.onComplete = null;

/**
 * If logEnabled is true, this is the logger that will be used.
**/
TestRunner.logger = MochiKit.Logging.logger;

/**
 * Toggle element visibility
**/
TestRunner._toggle = function(el) {
    if (el.className == "noshow") {
        el.className = "";
        el.style.cssText = "";
    } else {
        el.className = "noshow";
        el.style.cssText = "width:0px; height:0px; border:0px;";
    }
};


/**
 * Creates the iframe that contains a test
**/
TestRunner._makeIframe = function (url) {
    var iframe = document.createElement('iframe');
    iframe.src = url;
    iframe.name = url;
    iframe.width = "500";
    var tbody = TestRunner._summaryDiv.getElementsByTagName("tbody")[0];
    var tr = TR(null, TD({'colspan': '3'}, iframe));
    iframe._row = tr;
    tbody.appendChild(tr);
    return iframe;
};

/**
 * TestRunner entry point.
 *
 * The arguments are the URLs of the test to be ran.
 *
**/
TestRunner.runTests = function (/*url...*/) {
    if (TestRunner.logEnabled)
        TestRunner.logger.log("SimpleTest START");
  
    var body = document.getElementsByTagName("body")[0];
    appendChildNodes(body,
        TestRunner._testsDiv,
        TestRunner._progressDiv,
        TestRunner._summaryDiv
    );
    for (var i = 0; i < arguments.length; i++) {
        TestRunner._urls.push(arguments[i]); 
    }
    TestRunner.runNextTest();
};

/**
 * Run the next test. If no test remains, calls makeSummary
**/
TestRunner.runNextTest = function() {
    if (TestRunner._currentTest < TestRunner._urls.length) {
        var url = TestRunner._urls[TestRunner._currentTest];
        var progress = SPAN(null,
            "Running ", A({href:url}, url), "..."
        );
        
        if (TestRunner.logEnabled)
            TestRunner.logger.log(scrapeText(progress));
        
        TestRunner._progressDiv.appendChild(progress);
        TestRunner._iframes[url] = TestRunner._makeIframe(url);
    }  else {
        TestRunner.makeSummary();
        if (TestRunner.onComplete)
            TestRunner.onComplete();
    }
};

/**
 * This stub is called by SimpleTest when a test is finished.
**/
TestRunner.testFinished = function (doc) {
    appendChildNodes(TestRunner._progressDiv, SPAN(null, "Done"), BR());
    var finishedURL = TestRunner._urls[TestRunner._currentTest];
    
    if (TestRunner.logEnabled)
        TestRunner.logger.debug("SimpleTest finished " + finishedURL);
    
    TestRunner._iframeDocuments[finishedURL] = doc;
    // TestRunner._iframes[finishedURL].style.display = "none";
    TestRunner._toggle(TestRunner._iframes[finishedURL]);
    TestRunner._currentTest++;
    TestRunner.runNextTest();
};

/**
 * Display the summary in the browser
**/
TestRunner.makeSummary = function() {
    if (TestRunner.logEnabled)
        TestRunner.logger.log("SimpleTest FINISHED");
    var rows = [];
    for (var url in TestRunner._iframeDocuments) {
        var doc = TestRunner._iframeDocuments[url];
        var nOK = withDocument(doc,
            partial(getElementsByTagAndClassName, 'div', 'test_ok')
        ).length;
        var nNotOK = withDocument(doc,
            partial(getElementsByTagAndClassName, 'div', 'test_not_ok')
        ).length;
        var toggle = partial(TestRunner._toggle, TestRunner._iframes[url]);
        var jsurl = "TestRunner._toggle(TestRunner._iframes['" + url + "'])";
        var row = TR(
            {'style': {'backgroundColor': nNotOK > 0 ? "#f00":"#0f0"}}, 
            TD(null, url),
            TD(null, nOK),
            TD(null, nNotOK)
        );
        row.onclick = toggle;
        var tbody = TestRunner._summaryDiv.getElementsByTagName("tbody")[0];
        tbody.insertBefore(row, TestRunner._iframes[url]._row)
    }
};

if ( parent.SimpleTest && parent.runAJAXTest ) {
    TestRunner.makeSummary = function() {
        for (var url in TestRunner._iframeDocuments) {
            var doc = TestRunner._iframeDocuments[url];

            var OK = withDocument(doc, partial(getElementsByTagAndClassName, 'div', 'test_ok'));
            for ( var i = 0; i < OK.length; i++ )
                parent.SimpleTest.ok( true, OK[i].innerHTML );

            var NotOK = withDocument(doc, partial(getElementsByTagAndClassName, 'div', 'test_not_ok'));
            for ( var i = 0; i < NotOK.length; i++ )
                parent.SimpleTest.ok( false, NotOK[i].innerHTML );
        }

	parent.runAJAXTest();
    };
}
