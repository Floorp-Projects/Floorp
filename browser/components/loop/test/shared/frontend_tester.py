from marionette_test import MarionetteTestCase
import threading
import SimpleHTTPServer
import SocketServer
import BaseHTTPServer
import urlparse

PORT = 2222
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

        cls.server = ThreadingSimpleServer(('', PORT), handler)

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

    def set_server_prefix(self, srcdir_path=None):
        self.server_prefix = urlparse.urljoin("http://localhost:" + str(PORT),
                                              srcdir_path)

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

        raise AssertionError(self.get_failure_details())

    def get_failure_details(self):
        fail_nodes = self.marionette.find_elements("css selector",
                                                   '.test.fail')
        details = ["%d failure(s) encountered:" % len(fail_nodes)]
        for node in fail_nodes:
            details.append(
                node.find_element("tag name", 'h2').text.split("\n")[0])
            details.append(
                node.find_element("css selector", '.error').text)
        return "\n".join(details)
