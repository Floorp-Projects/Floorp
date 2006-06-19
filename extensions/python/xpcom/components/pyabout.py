# about:python, originally by Alex Badea
from xpcom import components, verbose
import sys, os
import platform

def getAbout():
    # Generate it each time so its always up-to-date.
    # Sort to keep things purdy
    mod_names = sys.modules.keys()
    mod_names.sort()
    env = os.environ.items()
    env.sort()
    return """
<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<html>
<head>
  <title>about:python</title>
</head>
<body>
<h1>about:python</h1>
<p> </p>
<p>Python %(version)s on %(platform)s</p>
<h2>resources</h2>
<p>Visit the <a href="http://developer.mozilla.org/en/docs/PyXPCOM">pyxpcom wiki.</a></p>
<h2>sys.path</h2><p>%(path)s</p><p> </p>
<h2>environment</h2><p>%(environment)s</p><p> </p>
<h2>modules</h2><p>%(modules)s</p><p> </p>

</body>
</html>
""" % {
    'version': sys.version,
    'platform': platform.platform(),
    'path': "<br>".join(sys.path),
    'environment': "<br>".join(["%s=%s" % (n,v) for n, v in env]),
    'modules': ", ".join(mod_names),
}


class AboutPython:
    _com_interfaces_ = components.interfaces.nsIAboutModule
    _reg_contractid_ = '@mozilla.org/network/protocol/about;1?what=python'
    _reg_clsid_ = '{6d5d462e-6de7-4bca-bbc6-c488d481351b}'
    _reg_desc_ = "about:python handler"

    def __init__(self):
        pass

    def newChannel(self, aURI):
        ioService = components.classes["@mozilla.org/network/io-service;1"] \
            .getService();

        istream = components.classes["@mozilla.org/io/string-input-stream;1"] \
            .createInstance()

        about = getAbout()
        istream.setData(about, len(about))

        channel = components.classes["@mozilla.org/network/input-stream-channel;1"] \
            .createInstance(components.interfaces.nsIInputStreamChannel)

        channel.setURI(aURI)
        #channel.contentType = "text/html"
        channel.contentStream = istream
        return channel

    def getURIFlags(self, aURI):
        return 0;
