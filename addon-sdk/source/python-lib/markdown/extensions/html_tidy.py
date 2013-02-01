#!/usr/bin/env python

"""
HTML Tidy Extension for Python-Markdown
=======================================

Runs [HTML Tidy][] on the output of Python-Markdown using the [uTidylib][] 
Python wrapper. Both libtidy and uTidylib must be installed on your system.

Note than any Tidy [options][] can be passed in as extension configs. So, 
for example, to output HTML rather than XHTML, set ``output_xhtml=0``. To
indent the output, set ``indent=auto`` and to have Tidy wrap the output in 
``<html>`` and ``<body>`` tags, set ``show_body_only=0``.

[HTML Tidy]: http://tidy.sourceforge.net/
[uTidylib]: http://utidylib.berlios.de/
[options]: http://tidy.sourceforge.net/docs/quickref.html

Copyright (c)2008 [Waylan Limberg](http://achinghead.com)

License: [BSD](http://www.opensource.org/licenses/bsd-license.php) 

Dependencies:
* [Python2.3+](http://python.org)
* [Markdown 2.0+](http://www.freewisdom.org/projects/python-markdown/)
* [HTML Tidy](http://utidylib.berlios.de/)
* [uTidylib](http://utidylib.berlios.de/)

"""

import markdown
import tidy

class TidyExtension(markdown.Extension):

    def __init__(self, configs):
        # Set defaults to match typical markdown behavior.
        self.config = dict(output_xhtml=1,
                           show_body_only=1,
                          )
        # Merge in user defined configs overriding any present if nessecary.
        for c in configs:
            self.config[c[0]] = c[1]

    def extendMarkdown(self, md, md_globals):
        # Save options to markdown instance
        md.tidy_options = self.config
        # Add TidyProcessor to postprocessors
        md.postprocessors['tidy'] = TidyProcessor(md)


class TidyProcessor(markdown.postprocessors.Postprocessor):

    def run(self, text):
        # Pass text to Tidy. As Tidy does not accept unicode we need to encode
        # it and decode its return value.
        return unicode(tidy.parseString(text.encode('utf-8'), 
                                        **self.markdown.tidy_options)) 


def makeExtension(configs=None):
    return TidyExtension(configs=configs)
