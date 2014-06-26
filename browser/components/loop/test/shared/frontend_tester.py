from marionette_test import MarionetteTestCase
import threading
import SimpleHTTPServer
import SocketServer
import BaseHTTPServer
import socket
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
class ThreadingSimpleServer(SocketServer.ThreadingMixIn,
                            BaseHTTPServer.HTTPServer):
    pass


class QuietHttpRequestHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):
    def log_message(self, format, *args, **kwargs):
        pass


class BaseTestFrontendUnits(MarionetteTestCase):

    @classmethod
    def setUpClass(cls):
        super(BaseTestFrontendUnits, cls).setUpClass()

        if DEBUG:
            handler = SimpleHTTPServer.SimpleHTTPRequestHandler
        else:
            handler = QuietHttpRequestHandler

        # Port 0 means to select an arbitrary unused port
        cls.server = ThreadingSimpleServer(('', 0), handler)
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

        # This extends the timeout for find_element to 10 seconds.
        # We need this as the tests take an amount of time to run after loading,
        # which we have to wait for.
        self.marionette.set_search_timeout(10000)

    # srcdir_path should be the directory relative to this file.
    def set_server_prefix(self, srcdir_path):
        # We may be run from a different path than topsrcdir, e.g. in the case
        # of packaged tests. If so, then we have to work out the right directory
        # for the local server.

        # First find the top of the working directory.
        commonPath = os.path.commonprefix([__file__, os.getcwd()])

        # Now get the relative path between the two
        self.relPath = os.path.relpath(os.path.dirname(__file__), commonPath)

        self.relPath = urllib.pathname2url(os.path.join(self.relPath, srcdir_path))

        # Finally join the relative path with the given src path
        self.server_prefix = urlparse.urljoin("http://localhost:" + str(self.port),
                                              self.relPath)

    def check_page(self, page):

        self.marionette.navigate(urlparse.urljoin(self.server_prefix, page))
        self.marionette.find_element("id", 'complete')

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
        #from ipdb import set_trace
        #set_trace()

        raise AssertionError(self.get_failure_details(page))

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
                "TEST-UNEXPECTED-FAIL | %s | %s - %s" % \
                (fullPageUrl, node.find_element("tag name", 'h2').text.split("\n")[0], errorText.split("\n")[0]))
            details.append(
                errorText)
        return "\n".join(details)
