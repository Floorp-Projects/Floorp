function test() {
    /** Test for Bug 396843 **/
    waitForExplicitFinish();

    function testInDocument(doc, documentID) {
        
        var allNodes = [];
        var XMLNodes = [];

        // HTML
        function HTML_TAG(name) {
            allNodes.push(doc.createElement(name));
        }

        /* List copy/pasted from nsHTMLTagList.h */
        HTML_TAG("a", "Anchor")
        HTML_TAG("abbr", "Span")
        HTML_TAG("acronym", "Span")
        HTML_TAG("address", "Span")
        HTML_TAG("applet", "SharedObject")
        HTML_TAG("area", "Area")
        HTML_TAG("b", "Span")
        HTML_TAG("base", "Shared")
        HTML_TAG("basefont", "Span")
        HTML_TAG("bdo", "Span")
        HTML_TAG("bgsound", "Span")
        HTML_TAG("big", "Span")
        HTML_TAG("blink", "Span")
        HTML_TAG("blockquote", "Shared")
        HTML_TAG("body", "Body")
        HTML_TAG("br", "BR")
        HTML_TAG("button", "Button")
        HTML_TAG("canvas", "Canvas")
        HTML_TAG("caption", "TableCaption")
        HTML_TAG("center", "Span")
        HTML_TAG("cite", "Span")
        HTML_TAG("code", "Span")
        HTML_TAG("col", "TableCol")
        HTML_TAG("colgroup", "TableCol")
        HTML_TAG("dd", "Span")
        HTML_TAG("del", "Mod")
        HTML_TAG("dfn", "Span")
        HTML_TAG("dir", "Shared")
        HTML_TAG("div", "Div")
        HTML_TAG("dl", "SharedList")
        HTML_TAG("dt", "Span")
        HTML_TAG("em", "Span")
        HTML_TAG("embed", "SharedObject")
        HTML_TAG("fieldset", "FieldSet")
        HTML_TAG("font", "Font")
        HTML_TAG("form", "Form")
        HTML_TAG("frame", "Frame")
        HTML_TAG("frameset", "FrameSet")
        HTML_TAG("h1", "Heading")
        HTML_TAG("h2", "Heading")
        HTML_TAG("h3", "Heading")
        HTML_TAG("h4", "Heading")
        HTML_TAG("h5", "Heading")
        HTML_TAG("h6", "Heading")
        HTML_TAG("head", "Head")
        HTML_TAG("hr", "HR")
        HTML_TAG("html", "Html")
        HTML_TAG("i", "Span")
        HTML_TAG("iframe", "IFrame")
        HTML_TAG("image", "")
        HTML_TAG("img", "Image")
        HTML_TAG("input", "Input")
        HTML_TAG("ins", "Mod")
        HTML_TAG("isindex", "Unknown")
        HTML_TAG("kbd", "Span")
        HTML_TAG("keygen", "Span")
        HTML_TAG("label", "Label")
        HTML_TAG("legend", "Legend")
        HTML_TAG("li", "LI")
        HTML_TAG("link", "Link")
        HTML_TAG("listing", "Span")
        HTML_TAG("map", "Map")
        HTML_TAG("marquee", "Div")
        HTML_TAG("menu", "Shared")
        HTML_TAG("meta", "Meta")
        HTML_TAG("multicol", "Unknown")
        HTML_TAG("nobr", "Span")
        HTML_TAG("noembed", "Div")
        HTML_TAG("noframes", "Div")
        HTML_TAG("noscript", "Div")
        HTML_TAG("object", "Object")
        HTML_TAG("ol", "SharedList")
        HTML_TAG("optgroup", "OptGroup")
        HTML_TAG("option", "Option")
        HTML_TAG("p", "Paragraph")
        HTML_TAG("param", "Shared")
        HTML_TAG("plaintext", "Span")
        HTML_TAG("pre", "Pre")
        HTML_TAG("q", "Shared")
        HTML_TAG("s", "Span")
        HTML_TAG("samp", "Span")
        HTML_TAG("script", "Script")
        HTML_TAG("select", "Select")
        HTML_TAG("small", "Span")
        HTML_TAG("spacer", "Unknown")
        HTML_TAG("span", "Span")
        HTML_TAG("strike", "Span")
        HTML_TAG("strong", "Span")
        HTML_TAG("style", "Style")
        HTML_TAG("sub", "Span")
        HTML_TAG("sup", "Span")
        HTML_TAG("table", "Table")
        HTML_TAG("tbody", "TableSection")
        HTML_TAG("td", "TableCell")
        HTML_TAG("textarea", "TextArea")
        HTML_TAG("tfoot", "TableSection")
        HTML_TAG("th", "TableCell")
        HTML_TAG("thead", "TableSection")
        HTML_TAG("template", "Template")
        HTML_TAG("title", "Title")
        HTML_TAG("tr", "TableRow")
        HTML_TAG("tt", "Span")
        HTML_TAG("u", "Span")
        HTML_TAG("ul", "SharedList")
        HTML_TAG("var", "Span")
        HTML_TAG("wbr", "Shared")
        HTML_TAG("xmp", "Span")

        function SVG_TAG(name) {
            allNodes.push(doc.createElementNS("http://www.w3.org/2000/svg", name));
        }

        // List sorta stolen from SVG element factory.
        SVG_TAG("a")
        SVG_TAG("polyline")
        SVG_TAG("polygon")
        SVG_TAG("circle")
        SVG_TAG("ellipse")
        SVG_TAG("line")
        SVG_TAG("rect")
        SVG_TAG("svg")
        SVG_TAG("g")
        SVG_TAG("foreignObject")
        SVG_TAG("path")
        SVG_TAG("text")
        SVG_TAG("tspan")
        SVG_TAG("image")
        SVG_TAG("style")
        SVG_TAG("linearGradient")
        SVG_TAG("metadata")
        SVG_TAG("radialGradient")
        SVG_TAG("stop")
        SVG_TAG("defs")
        SVG_TAG("desc")
        SVG_TAG("script")
        SVG_TAG("use")
        SVG_TAG("symbol")
        SVG_TAG("marker")
        SVG_TAG("title")
        SVG_TAG("clipPath")
        SVG_TAG("textPath")
        SVG_TAG("filter")
        SVG_TAG("feBlend")
        SVG_TAG("feColorMatrix")
        SVG_TAG("feComponentTransfer")
        SVG_TAG("feComposite")
        SVG_TAG("feFuncR")
        SVG_TAG("feFuncG")
        SVG_TAG("feFuncB")
        SVG_TAG("feFuncA")
        SVG_TAG("feGaussianBlur")
        SVG_TAG("feMerge")
        SVG_TAG("feMergeNode")
        SVG_TAG("feMorphology")
        SVG_TAG("feOffset")
        SVG_TAG("feFlood")
        SVG_TAG("feTile")
        SVG_TAG("feTurbulence")
        SVG_TAG("feConvolveMatrix")
        SVG_TAG("feDistantLight")
        SVG_TAG("fePointLight")
        SVG_TAG("feSpotLight")
        SVG_TAG("feDiffuseLighting")
        SVG_TAG("feSpecularLighting")
        SVG_TAG("feDisplacementMap")
        SVG_TAG("feImage")
        SVG_TAG("pattern")
        SVG_TAG("mask")
        SVG_TAG("svgSwitch")

        // Toss in some other namespaced stuff too, for good measure
        // XUL stuff might not be creatable in content documents
        try {
            allNodes.push(doc.createElementNS(
                "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
                "window"));
        } catch (e) {}
        allNodes.push(doc.createElementNS("http://www.w3.org/1998/Math/MathML",
                                          "math"));
        allNodes.push(doc.createElementNS("http://www.w3.org/2001/xml-events",
                                          "testname"));
        allNodes.push(doc.createElementNS("bogus.namespace", "testname"));

        var XMLDoc = doc.implementation.createDocument("", "", null);

        // And non-elements
        allNodes.push(doc.createTextNode("some text"));
        allNodes.push(doc.createComment("some text"));
        allNodes.push(doc.createDocumentFragment());
        XMLNodes.push(XMLDoc.createCDATASection("some text"));
        XMLNodes.push(XMLDoc.createProcessingInstruction("PI", "data"));
        
        function runTestUnwrapped() {
            if (!("wrappedJSObject" in doc)) {
                return;
            }
            ok(doc.wrappedJSObject.nodePrincipal === undefined,
               "Must not have document principal for " + documentID);
            ok(doc.wrappedJSObject.baseURIObject === undefined,
               "Must not have document base URI for " + documentID);
            ok(doc.wrappedJSObject.documentURIObject === undefined,
               "Must not have document URI for " + documentID);

            for (var i = 0; i < allNodes.length; ++i) {
                ok(allNodes[i].wrappedJSObject.nodePrincipal === undefined,
                   "Unexpected principal appears for " + allNodes[i].nodeName +
                   " in " + documentID);
                ok(allNodes[i].wrappedJSObject.baseURIObject === undefined,
                   "Unexpected base URI appears for " + allNodes[i].nodeName +
                   " in " + documentID);
            }
        }
        
        function runTestProps() {
            isnot(doc.nodePrincipal, null,
                  "Must have document principal in " + documentID);
            is(doc.nodePrincipal instanceof Components.interfaces.nsIPrincipal,
               true, "document principal must be a principal in " + documentID);
            isnot(doc.baseURIObject, null,
                  "Must have document base URI in" + documentID);
            is(doc.baseURIObject instanceof Components.interfaces.nsIURI,
               true, "document base URI must be a URI in " + documentID);
            isnot(doc.documentURIObject, null,
                  "Must have document URI " + documentID);
            is(doc.documentURIObject instanceof Components.interfaces.nsIURI,
               true, "document URI must be a URI in " + documentID);
            is(doc.documentURIObject.spec, doc.documentURI,
               "document URI must be the right URI in " + documentID);
     
            for (var i = 0; i < allNodes.length; ++i) {
                is(allNodes[i].nodePrincipal, doc.nodePrincipal,
                   "Unexpected principal for " + allNodes[i].nodeName +
                   " in " + documentID);
                is(allNodes[i].baseURIObject, doc.baseURIObject,
                   "Unexpected base URI for " + allNodes[i].nodeName +
                   " in " + documentID);
            }

            for (i = 0; i < XMLNodes.length; ++i) {
                is(XMLNodes[i].nodePrincipal, doc.nodePrincipal,
                   "Unexpected principal for " + XMLNodes[i].nodeName +
                   " in " + documentID);
                is(XMLNodes[i].baseURIObject.spec, "about:blank",
                   "Unexpected base URI for " + XMLNodes[i].nodeName +
                   " in " + documentID);
            }
        }

        runTestUnwrapped();
        runTestProps();
        runTestUnwrapped();
    }

    var testsRunning = 2;
    
    testInDocument(document, "browser window");

    function newTabTest(url) {
        var newTab = gBrowser.addTab();
        var newBrowser = gBrowser.getBrowserForTab(newTab);
        newBrowser.contentWindow.location.href = url;
        function testBrowser(event) {
            newBrowser.removeEventListener("load", testBrowser, true);
            is(event.target, newBrowser.contentDocument,
               "Unexpected target in " + url);
            testInDocument(newBrowser.contentDocument, url);

            gBrowser.removeTab(newTab);

            --testsRunning;
            if (testsRunning == 0) {
                finish();
            }
        }
        newBrowser.addEventListener("load", testBrowser, true);
    }

    newTabTest("about:blank");
    newTabTest("about:config");
}
