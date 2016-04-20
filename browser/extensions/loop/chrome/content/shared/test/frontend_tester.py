from marionette import MarionetteTestCase
from marionette_driver.errors import NoSuchElementException
import threading
import SimpleHTTPServer
import SocketServer
import BaseHTTPServer
import urllib
import urlparse
import os

DEBUG = False


# XXX Once we're on a branch with bug 993478 landed, we may want to get
# rid of this HTTP server and just use the built-in one from Marionette,
# since there will less code to maintain, and it will be faster.  We'll
# need to consider whether this code wants to be shared with WebDriver tests
# for other browsers, though.
#

# This is the common directory segment, what you need to get from the
# common path to the relative path location. Used to get the redirects
# correct both locally and on the build systems.
gCommonDir = None

# These redirects map the paths expected by the index.html files to the paths
# in mozilla-central. In the github repo, the unit tests are run entirely within
# karma, where the files are loaded directly. For mozilla-central we must map
# the files to the appropriate place.
REDIRECTIONS = {
    "/test/vendor": "/chrome/content/shared/test/vendor",
    "/shared/js": "/chrome/content/shared/js",
    "/shared/test": "/chrome/content/shared/test",
    "/shared/vendor": "/chrome/content/shared/vendor",
    "/add-on/panels/vendor": "/chrome/content/panels/vendor",
    "/add-on/panels/js": "/chrome/content/panels/js",
    "/add-on/panels/test": "/chrome/content/panels/test",
    "/add-on/shared/js": "/chrome/content/shared/js",
    "/add-on/shared/vendor": "/chrome/content/shared/vendor",
}


class ThreadingSimpleServer(SocketServer.ThreadingMixIn,
                            BaseHTTPServer.HTTPServer):
    pass


# Overrides the simpler HTTP server handler to introduce redirects to map
# files that are opened using absolete paths to ones which are relative
# to the source tree.
class HttpRequestHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):
    def do_HEAD(s):
        lastSlash = s.path.rfind("/")
        path = s.path[:lastSlash]

        if (path in REDIRECTIONS):
            filename = s.path[lastSlash:]

            s.send_response(301)
            # Prefix the redirections with the common directory segment.
            s.send_header("Location", "/" + gCommonDir + REDIRECTIONS.get(path, "/") + filename)
            s.end_headers()
        else:
            SimpleHTTPServer.SimpleHTTPRequestHandler.do_HEAD(s)

    def do_GET(s):
        lastSlash = s.path.rfind("/")
        path = s.path[:lastSlash]

        if (path in REDIRECTIONS):
            s.do_HEAD()
        else:
            SimpleHTTPServer.SimpleHTTPRequestHandler.do_GET(s)

    def log_message(self, format, *args, **kwargs):
        if DEBUG:
            BaseHTTPServer.BaseHTTPRequestHandler.log_message(self, format, *args, **kwargs)
        else:
            pass


class BaseTestFrontendUnits(MarionetteTestCase):

    @classmethod
    def setUpClass(cls):
        super(BaseTestFrontendUnits, cls).setUpClass()

        # Port 0 means to select an arbitrary unused port
        cls.server = ThreadingSimpleServer(('', 0), HttpRequestHandler)
        cls.ip, cls.port = cls.server.server_address

        cls.server_thread = threading.Thread(target=cls.server.serve_forever)
        cls.server_thread.daemon = False
        cls.server_thread.start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server_thread.join()

        # make sure everything gets GCed so it doesn't interfere with the next
        # test class.  Even though this is class-static, each subclass gets
        # its own instance of this stuff.
        cls.server_thread = None
        cls.server = None

    def setUp(self):
        super(BaseTestFrontendUnits, self).setUp()

        # Unfortunately, enforcing preferences currently comes with the side
        # effect of launching and restarting the browser before running the
        # real functional tests.  Bug 1048554 has been filed to track this.
        #
        # Note: when e10s is enabled by default, this pref can go away. The automatic
        # restart will also go away if this is still the only pref set here.
        self.marionette.enforce_gecko_prefs({
            "browser.tabs.remote.autostart": True
        })

        # This extends the timeout for find_element. We need this as the tests
        # take an amount of time to run after loading, which we have to wait for.
        self.marionette.set_search_timeout(120000)

        self.marionette.timeouts("page load", 120000)

    # srcdir_path should be the directory relative to this file.
    def set_server_prefix(self, srcdir_path):
        global gCommonDir

        # We may be run from a different path than topsrcdir, e.g. in the case
        # of packaged tests. If so, then we have to work out the right directory
        # for the local server.

        # First find the top of the working directory.
        commonPath = os.path.commonprefix([__file__, os.getcwd()])

        # Now get the relative path between the two
        self.relPath = os.path.relpath(os.path.dirname(__file__), commonPath)

        self.relPath = urllib.pathname2url(os.path.join(self.relPath, srcdir_path))

        # This is the common directory segment, what you need to get from the
        # common path to the relative path location. Used to get the redirects
        # correct both locally and on the build systems.
        # The .replace is to change windows path slashes to unix like ones for
        # the url.
        gCommonDir = os.path.normpath(self.relPath).replace("\\", "//")

        # Finally join the relative path with the given src path
        self.server_prefix = urlparse.urljoin("http://localhost:" + str(self.port),
                                              self.relPath)

    def check_page(self, page):

        self.marionette.navigate(urlparse.urljoin(self.server_prefix, page))
        try:
            self.marionette.find_element("id", 'complete')
        except NoSuchElementException:
            fullPageUrl = urlparse.urljoin(self.relPath, page)

            details = "%s: 1 failure encountered\n%s" % \
                      (fullPageUrl,
                       self.get_failure_summary(
                           fullPageUrl, "Waiting for Completion",
                           "Could not find the test complete indicator"))

            raise AssertionError(details)

        fail_node = self.marionette.find_element("css selector",
                                                 '.failures > em')
        if fail_node.text == "0":
            return

        # This may want to be in a more general place triggerable by an env
        # var some day if it ends up being something we need often:
        #
        # If you have browser-based unit tests which work when loaded manually
        # but not from marionette, uncomment the two lines below to break
        # on failing tests, so that the browsers won't be torn down, and you
        # can use the browser debugging facilities to see what's going on.
        #
        # from ipdb import set_trace
        # set_trace()

        raise AssertionError(self.get_failure_details(page))

    def get_failure_summary(self, fullPageUrl, testName, testError):
        return "TEST-UNEXPECTED-FAIL | %s | %s - %s" % (fullPageUrl, testName, testError)

    def get_failure_details(self, page):
        fail_nodes = self.marionette.find_elements("css selector",
                                                   '.test.fail')
        fullPageUrl = urlparse.urljoin(self.relPath, page)

        details = ["%s: %d failure(s) encountered:" % (fullPageUrl, len(fail_nodes))]

        for node in fail_nodes:
            errorText = node.find_element("css selector", '.error').text

            # We have to work our own failure message here, as we could be reporting multiple failures.
            # XXX Ideally we'd also give the full test tree for <test name> - that requires walking
            # up the DOM tree.

            # Format: TEST-UNEXPECTED-FAIL | <filename> | <test name> - <test error>
            details.append(
                self.get_failure_summary(page,
                                         node.find_element("tag name", 'h2').text.split("\n")[0],
                                         errorText.split("\n")[0]))
            details.append(
                errorText)
        return "\n".join(details)
