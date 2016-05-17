"use strict"; /*
               * Many of these tests are ported from Autolinker.js:
               *
               * https://github.com/gregjacobs/Autolinker.js/blob/master/tests/AutolinkerSpec.js
               *
               * which is released under the MIT license.  Thanks to Greg Jacobs for his hard
               * work there.
               *
               * The MIT License (MIT)
               * Original Work Copyright (c) 2014 Gregory Jacobs (http://greg-jacobs.com)
               *
               * Permission is hereby granted, free of charge, to any person obtaining
               * a copy of this software and associated documentation files (the
               * "Software"), to deal in the Software without restriction, including
               * without limitation the rights to use, copy, modify, merge, publish,
               * distribute, sublicense, and/or sell copies of the Software, and to
               * permit persons to whom the Software is furnished to do so, subject to the
               * following conditions:
               *
               * The above copyright notice and this permission notice shall be included
               * in all copies or substantial portions of the Software.
               *
               * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
               * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
               * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
               * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
               * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
               * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
               * SOFTWARE.
               */
describe("loop.shared.views.LinkifiedTextView", function () {
  "use strict";

  var expect = chai.expect;
  var LinkifiedTextView = loop.shared.views.LinkifiedTextView;
  var TestUtils = React.addons.TestUtils;

  describe("LinkifiedTextView", function () {
    function renderToMarkup(string, extraProps) {
      return ReactDOMServer.renderToStaticMarkup(
      React.createElement(
      LinkifiedTextView, 
      _.extend({ rawText: string }, extraProps)));}


    describe("#render", function () {
      function testRender(testData) {
        it(testData.desc, function () {
          var markup = renderToMarkup(testData.rawText, 
          { suppressTarget: true, sendReferrer: true });

          expect(markup).to.equal(testData.markup);});}



      function testSkip(testData) {
        it.skip(testData.desc, function () {
          var markup = renderToMarkup(testData.rawText, 
          { suppressTarget: true, sendReferrer: true });

          expect(markup).to.equal(testData.markup);});}



      describe("this.props.suppressTarget", function () {
        it("should make links w/o a target attr if suppressTarget is true", 
        function () {
          var markup = renderToMarkup("http://example.com", { suppressTarget: true });

          expect(markup).to.equal(
          '<p><a href="http://example.com/" rel="noreferrer">http://example.com/</a></p>');});


        it("should make links with target=_blank if suppressTarget is not given", 
        function () {
          var markup = renderToMarkup("http://example.com", {});

          expect(markup).to.equal(
          '<p><a href="http://example.com/" rel="noreferrer" target="_blank">http://example.com/</a></p>');});});



      describe("this.props.sendReferrer", function () {
        it("should make links w/o rel=noreferrer if sendReferrer is true", 
        function () {
          var markup = renderToMarkup("http://example.com", { sendReferrer: true });

          expect(markup).to.equal(
          '<p><a href="http://example.com/" target="_blank">http://example.com/</a></p>');});


        it("should make links with rel=noreferrer if sendReferrer is not given", 
        function () {
          var markup = renderToMarkup("http://example.com", {});

          expect(markup).to.equal(
          '<p><a href="http://example.com/" rel="noreferrer" target="_blank">http://example.com/</a></p>');});});



      describe("this.props.linkClickHandler", function () {
        function mountTestComponent(string, extraProps) {
          return TestUtils.renderIntoDocument(
          React.createElement(
          LinkifiedTextView, 
          _.extend({ rawText: string }, extraProps)));}


        it("should be called when a generated link is clicked", function () {
          var fakeUrl = "http://example.com";
          var linkClickHandler = sinon.stub();
          var comp = mountTestComponent(fakeUrl, { linkClickHandler: linkClickHandler });

          TestUtils.Simulate.click(ReactDOM.findDOMNode(comp).querySelector("a"));

          sinon.assert.calledOnce(linkClickHandler);});


        it("should cause sendReferrer and suppressTarget props to be ignored", 
        function () {
          var linkClickHandler = function linkClickHandler() {};

          var markup = renderToMarkup("http://example.com", { 
            linkClickHandler: linkClickHandler, 
            sendReferrer: false, 
            suppressTarget: false });


          expect(markup).to.equal(
          '<p><a href="http://example.com/">http://example.com/</a></p>');});


        describe("#_handleClickEvent", function () {
          var fakeEvent;
          var fakeUrl = "http://example.com";

          beforeEach(function () {
            fakeEvent = { 
              currentTarget: { href: fakeUrl }, 
              preventDefault: sinon.stub(), 
              stopPropagation: sinon.stub() };});



          it("should call preventDefault on the given event", function () {
            function linkClickHandler() {}
            var comp = mountTestComponent(
            fakeUrl, { linkClickHandler: linkClickHandler });

            comp._handleClickEvent(fakeEvent);

            sinon.assert.calledOnce(fakeEvent.preventDefault);
            sinon.assert.calledWithExactly(fakeEvent.stopPropagation);});


          it("should call stopPropagation on the given event", function () {
            function linkClickHandler() {}
            var comp = mountTestComponent(
            fakeUrl, { linkClickHandler: linkClickHandler });

            comp._handleClickEvent(fakeEvent);

            sinon.assert.calledOnce(fakeEvent.stopPropagation);
            sinon.assert.calledWithExactly(fakeEvent.stopPropagation);});


          it("should call this.props.linkClickHandler with event.currentTarget.href", function () {
            var linkClickHandler = sinon.stub();
            var comp = mountTestComponent(
            fakeUrl, { linkClickHandler: linkClickHandler });

            comp._handleClickEvent(fakeEvent);

            sinon.assert.calledOnce(linkClickHandler);
            sinon.assert.calledWithExactly(linkClickHandler, fakeUrl);});});});




      // Note that these are really integration tests with the parser and React.
      // Since we're depending on that integration to provide us with security
      // against various injection problems, it feels fairly important.  That
      // said, these tests are not terribly robust in the face of markup changes
      // in the code, and over time, some of them may want to be pushed down
      // to only be unit tests against the parser or against
      // parseStringToElements.  We may also want both unit and integration
      // testing for some subset of these.
      var tests = [
      { 
        desc: "should only add a container to a string with no URLs", 
        rawText: "This is a test.", 
        markup: "<p>This is a test.</p>" }, 

      { 
        desc: "should linkify a string containing only a URL with a trailing slash", 
        rawText: "http://example.com/", 
        markup: '<p><a href="http://example.com/">http://example.com/</a></p>' }, 

      { 
        desc: "should linkify a string containing only a URL with no trailing slash", 
        rawText: "http://example.com", 
        markup: '<p><a href="http://example.com/">http://example.com/</a></p>' }, 

      { 
        desc: "should linkify a URL with text preceding it", 
        rawText: "This is a link to http://example.com", 
        markup: '<p>This is a link to <a href="http://example.com/">http://example.com/</a></p>' }, 

      { 
        desc: "should linkify a URL with text before and after", 
        rawText: "Look at http://example.com near the bottom", 
        markup: '<p>Look at <a href="http://example.com/">http://example.com/</a> near the bottom</p>' }, 

      { 
        desc: "should linkify an http URL", 
        rawText: "This is an http://example.com test", 
        markup: '<p>This is an <a href="http://example.com/">http://example.com/</a> test</p>' }, 

      { 
        desc: "should linkify an https URL", 
        rawText: "This is an https://example.com test", 
        markup: '<p>This is an <a href="https://example.com/">https://example.com/</a> test</p>' }, 

      { 
        desc: "should not linkify a data URL", 
        rawText: "This is an data:image/png;base64,iVBORw0KGgoAAA test", 
        markup: "<p>This is an data:image/png;base64,iVBORw0KGgoAAA test</p>" }, 

      { 
        desc: "should linkify URLs with a port number and no trailing slash", 
        rawText: "Joe went to http://example.com:8000 today.", 
        markup: '<p>Joe went to <a href="http://example.com:8000/">http://example.com:8000/</a> today.</p>' }, 

      { 
        desc: "should linkify URLs with a port number and a trailing slash", 
        rawText: "Joe went to http://example.com:8000/ today.", 
        markup: '<p>Joe went to <a href="http://example.com:8000/">http://example.com:8000/</a> today.</p>' }, 

      { 
        desc: "should linkify URLs with a port number and a path", 
        rawText: "Joe went to http://example.com:8000/mysite/page today.", 
        markup: '<p>Joe went to <a href="http://example.com:8000/mysite/page">http://example.com:8000/mysite/page</a> today.</p>' }, 

      { 
        desc: "should linkify URLs with a port number and a query string", 
        rawText: "Joe went to http://example.com:8000?page=index today.", 
        markup: '<p>Joe went to <a href="http://example.com:8000/?page=index">http://example.com:8000/?page=index</a> today.</p>' }, 

      { 
        desc: "should linkify a URL with a port number and a hash string", 
        rawText: "Joe went to http://example.com:8000#page=index today.", 
        markup: '<p>Joe went to <a href="http://example.com:8000/#page=index">http://example.com:8000/#page=index</a> today.</p>' }, 

      { 
        desc: "should NOT include preceding ':' intros without a space", 
        rawText: "the link:http://example.com/", 
        markup: '<p>the link:<a href="http://example.com/">http://example.com/</a></p>' }, 

      { 
        desc: "should NOT autolink URLs with 'javascript:' URI scheme", 
        rawText: "do not link javascript:window.alert('hi') please", 
        markup: "<p>do not link javascript:window.alert(&#x27;hi&#x27;) please</p>" }, 

      { 
        desc: "should NOT autolink URLs with the 'JavAscriPt:' scheme", 
        rawText: "do not link JavAscriPt:window.alert('hi') please", 
        markup: "<p>do not link JavAscriPt:window.alert(&#x27;hi&#x27;) please</p>" }, 

      { 
        desc: "should NOT autolink possible URLs with the 'vbscript:' URI scheme", 
        rawText: "do not link vbscript:window.alert('hi') please", 
        markup: "<p>do not link vbscript:window.alert(&#x27;hi&#x27;) please</p>" }, 

      { 
        desc: "should NOT autolink URLs with the 'vBsCriPt:' URI scheme", 
        rawText: "do not link vBsCriPt:window.alert('hi') please", 
        markup: "<p>do not link vBsCriPt:window.alert(&#x27;hi&#x27;) please</p>" }, 

      { 
        desc: "should NOT autolink a string in the form of 'version:1.0'", 
        rawText: "version:1.0", 
        markup: "<p>version:1.0</p>" }, 

      { 
        desc: "should linkify an ftp URL", 
        rawText: "This is an ftp://example.com test", 
        markup: '<p>This is an <a href="ftp://example.com/">ftp://example.com/</a> test</p>' }, 


      // We don't want to include trailing dots in URLs, even though those
      // are valid DNS names, as that should match user intent more of the
      // time, as well as avoid this stuff:
      //
      // http://saynt2day.blogspot.it/2013/03/danger-of-trailing-dot-in-domain-name.html
      //
      { 
        desc: "should linkify 'http://example.com.', w/o a trailing dot", 
        rawText: "Joe went to http://example.com.", 
        markup: '<p>Joe went to <a href="http://example.com/">http://example.com/</a>.</p>' }, 

      // XXX several more tests like this we could port from Autolinkify.js
      // see https://bugzilla.mozilla.org/show_bug.cgi?id=1186254
      { 
        desc: "should exclude invalid chars after domain part", 
        rawText: "Joe went to http://www.example.com's today", 
        markup: '<p>Joe went to <a href="http://www.example.com/">http://www.example.com/</a>&#x27;s today</p>' }, 

      { 
        desc: "should not linkify protocol-relative URLs", 
        rawText: "//C/Programs", 
        markup: "<p>//C/Programs</p>" }, 

      { 
        desc: "should not linkify malformed URI sequences", 
        rawText: "http://www.example.com/DNA/pizza/menu/lots-of-different-kinds-of-pizza/%8D%E0%B8%88%E0%B8%A1%E0%B8%A3%E0%8D%E0%B8%88%E0%B8%A1%E0%B8%A3%E0%", 
        markup: "<p>http://www.example.com/DNA/pizza/menu/lots-of-different-kinds-of-pizza/%8D%E0%B8%88%E0%B8%A1%E0%B8%A3%E0%8D%E0%B8%88%E0%B8%A1%E0%B8%A3%E0%</p>" }, 

      // do a few tests to convince ourselves that, when our code is handled
      // HTML in the input box, the integration of our code with React should
      // cause that to rendered to appear as HTML source code, rather than
      // getting injected into our real HTML DOM
      { 
        desc: "should linkify simple HTML include an href properly escaped", 
        rawText: '<p>Joe went to <a href="http://www.example.com">example</a></p>', 
        markup: '<p>&lt;p&gt;Joe went to &lt;a href=&quot;<a href="http://www.example.com/">http://www.example.com/</a>&quot;&gt;example&lt;/a&gt;&lt;/p&gt;</p>' }, 

      { 
        desc: "should linkify HTML with nested tags and resource path properly escaped", 
        rawText: '<a href="http://example.com"><img src="http://example.com" /></a>', 
        markup: '<p>&lt;a href=&quot;<a href="http://example.com/">http://example.com/</a>&quot;&gt;&lt;img src=&quot;<a href="http://example.com/">http://example.com/</a>&quot; /&gt;&lt;/a&gt;</p>' }, 

      { 
        desc: "should linkify and decode a string containing a Homographic attack URL with no trailing slash", 
        rawText: "http://ebаy.com", 
        markup: '<p><a href="http://xn--eby-7cd.com/">http://xn--eby-7cd.com/</a></p>' }];



      var skippedTests = [

      // XXX lots of tests similar to below we could port:
      // see https://bugzilla.mozilla.org/show_bug.cgi?id=1186254
      { 
        desc: "should link localhost URLs with an allowed URL scheme", 
        rawText: "Joe went to http://localhost today", 
        markup: '<p>Joe went to <a href="http://localhost">localhost</a></p> today' }, 

      // XXX lots of tests similar to below we could port:
      // see https://bugzilla.mozilla.org/show_bug.cgi?id=1186254
      { 
        desc: "should not include a ? if at the end of a URL", 
        rawText: "Did Joe go to http://example.com?", 
        markup: '<p>Did Joe go to <a href="http://example.com/">http://example.com/</a>?</p>' }, 

      { 
        desc: "should linkify 'check out http://example.com/monkey.', w/o trailing dots", 
        rawText: "check out http://example.com/monkey...", 
        markup: '<p>check out <a href="http://example.com/monkey">http://example.com/monkey</a>...</p>' }, 

      // another variant of eating too many trailing characters, it includes
      // the trailing ", which it shouldn't.  Makes links inside pasted HTML
      // source be slightly broken. Not key for our target users, I suspect,
      // but still...
      { 
        desc: "should linkify HTML with nested tags and a resource path properly escaped", 
        rawText: '<a href="http://example.com"><img src="http://example.com/someImage.jpg" /></a>', 
        markup: '<p>&lt;a href=&quot;<a href="http://example.com/">http://example.com/</a>&quot;&gt;&lt;img src=&quot;<a href="http://example.com/someImage.jpg&quot;">http://example.com/someImage.jpg&quot;</a> /&gt;&lt;/a&gt;</p>' }, 

      // XXX handle domains without schemes (bug 1186245)
      // see https://bugzilla.mozilla.org/show_bug.cgi?id=1186254
      { 
        desc: "should linkify a.museum (known TLD), but not abc.qqqq", 
        rawText: "a.museum should be linked, but abc.qqqq should not", 
        markup: '<p><a href="http://a.museum">a.museum</a> should be linked, but abc.qqqq should not</p>' }, 

      { 
        desc: "should linkify example.xyz (known TLD), but not example.etc (unknown TLD)", 
        rawText: "example.xyz should be linked, example.etc should not", 
        rawMarkup: '<><a href="http://example.xyz">example.xyz</a> should be linked, example.etc should not</p>' }];



      tests.forEach(testRender);

      // XXX Over time, we'll want to make these pass..
      // see https://bugzilla.mozilla.org/show_bug.cgi?id=1186254

      skippedTests.forEach(testSkip);});});});
