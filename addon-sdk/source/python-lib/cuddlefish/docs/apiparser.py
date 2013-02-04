# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys, re, textwrap

VERSION = 4

class ParseError(Exception):
    # args[1] is the line number that caused the problem
    def __init__(self, why, lineno):
        self.why = why
        self.lineno = lineno
    def __str__(self):
        return ("ParseError: the JS API docs were unparseable on line %d: %s" %
                        (self.lineno, self.why))

class Accumulator:
    def __init__(self, holder, firstline):
        self.holder = holder
        self.firstline = firstline
        self.otherlines = []
    def addline(self, line):
        self.otherlines.append(line)
    def finish(self):
        # take a list of strings like:
        #    "initial stuff"    (this is in firstline)
        #    "  more stuff"     (this is in lines[0])
        #    "  yet more stuff"
        #    "      indented block"
        #    "      indented block"
        #    "  nonindented stuff"  (lines[-1])
        #
        # calculate the indentation level by looking at all but the first
        # line, and removing the whitespace they all have in common. Then
        # join the results with newlines and return a single string.
        pieces = []
        if self.firstline:
            pieces.append(self.firstline)
        if self.otherlines:
            pieces.append(textwrap.dedent("\n".join(self.otherlines)))
        self.holder["description"] = "\n".join(pieces)


class APIParser:
    def parse(self, lines, lineno):
        api = {"line_number": lineno + 1}
# assign the name from the first line, of the form "<api name="API_NAME">"
        title_line = lines[lineno].rstrip("\n")
        api["name"] = self._parse_title_line(title_line, lineno + 1)
        lineno += 1
# finished with the first line, assigned the name
        working_set = self._initialize_working_set()
        props = []
        currentPropHolder = api
# fetch the next line, of the form "@tag [name] {datatype} description"
# and parse it into tag, info, description
        tag, info, firstline = self._parseTypeLine(lines[lineno], lineno + 1)
        api["type"] = tag
# if this API element is a property then datatype must be set
        if tag == 'property':
            api['datatype'] = info['datatype']
        # info is ignored
        currentAccumulator = Accumulator(api, firstline)
        lineno += 1
        while (lineno) < len(lines):
            line = lines[lineno].rstrip("\n")
            # accumulate any multiline descriptive text belonging to
            # the preceding "@" section
            if self._is_description_line(line):
                currentAccumulator.addline(line)
            else:
                currentAccumulator.finish()
                if line.startswith("<api"):
                # then we should recursively handle a nested element
                    nested_api, lineno = self.parse(lines, lineno)
                    self._update_working_set(nested_api, working_set)
                elif line.startswith("</api"):
                # then we have finished parsing this api element
                    currentAccumulator.finish()
                    if props and currentPropHolder:
                        currentPropHolder["props"] = props
                    self._assemble_api_element(api, working_set)
                    return api, lineno
                else:
                # then we are looking at a subcomponent of an <api> element
                    tag, info, desc = self._parseTypeLine(line, lineno + 1)
                    currentAccumulator = Accumulator(info, desc)
                    if tag == "prop":
                        # build up props[]
                        props.append(info)
                    elif tag == "returns":
                        # close off the @prop list
                        if props and currentPropHolder:
                            currentPropHolder["props"] = props
                            props = []
                        api["returns"] = info
                        currentPropHolder = info
                    elif tag == "param":
                        # close off the @prop list
                        if props and currentPropHolder:
                            currentPropHolder["props"] = props
                            props = []
                        working_set["params"].append(info)
                        currentPropHolder = info
                    elif tag == "argument":
                        # close off the @prop list
                        if props and currentPropHolder:
                            currentPropHolder["props"] = props
                            props = []
                        working_set["arguments"].append(info)
                        currentPropHolder = info
                    else:
                        raise ParseError("unknown '@' section header %s in \
                                           '%s'" % (tag, line), lineno + 1)
            lineno += 1
        raise ParseError("closing </api> tag not found for <api name=\"" +
                         api["name"] + "\">", lineno + 1)

    def _parse_title_line(self, title_line, lineno):
        if "name" not in title_line:
            raise ParseError("Opening <api> tag must have a name attribute.",
                            lineno)
        m = re.search("name=['\"]{0,1}([-\w\.]*?)['\"]", title_line)
        if not m:
            raise ParseError("No value for name attribute found in "
                                     "opening <api> tag.", lineno)
        return m.group(1)

    def _is_description_line(self, line):
        return not ( (line.lstrip().startswith("@")) or
               (line.lstrip().startswith("<api")) or
               (line.lstrip().startswith("</api")) )

    def _initialize_working_set(self):
        # working_set accumulates api elements
        # that might belong to a parent api element
        working_set = {}
        working_set["constructors"] = []
        working_set["methods"] = []
        working_set["properties"] = []
        working_set["params"] = []
        working_set["events"] = []
        working_set["arguments"] = []
        return working_set

    def _update_working_set(self, nested_api, working_set):
        # add this api element to whichever list is appropriate
        if nested_api["type"] == "constructor":
            working_set["constructors"].append(nested_api)
        if nested_api["type"] == "method":
            working_set["methods"].append(nested_api)
        if nested_api["type"] == "property":
            working_set["properties"].append(nested_api)
        if nested_api["type"] == "event":
            working_set["events"].append(nested_api)

    def _assemble_signature(self, api_element, params):
        signature = api_element["name"] + "("
        if len(params) > 0:
            signature += params[0]["name"]
            for param in params[1:]:
                signature += ", " + param["name"]
        signature += ")"
        api_element["signature"] = signature

    def _assemble_api_element(self, api_element, working_set):
        # if any of this working set's lists are non-empty,
        # add it to the current api element
        if (api_element["type"] == "constructor") or \
           (api_element["type"] == "function") or \
           (api_element["type"] == "method"):
           self._assemble_signature(api_element, working_set["params"])
        if len(working_set["params"]) > 0:
            api_element["params"] = working_set["params"]
        if len(working_set["properties"]) > 0:
            api_element["properties"] = working_set["properties"]
        if len(working_set["constructors"]) > 0:
            api_element["constructors"] = working_set["constructors"]
        if len(working_set["methods"]) > 0:
            api_element["methods"] = working_set["methods"]
        if len(working_set["events"]) > 0:
            api_element["events"] = working_set["events"]
        if len(working_set["arguments"]) > 0:
            api_element["arguments"] = working_set["arguments"]

    def _validate_info(self, tag, info, line, lineno):
        if tag == 'property':
            if not 'datatype' in info:
                raise ParseError("No type found for @property.", lineno)
        elif tag == "param":
            if info.get("required", False) and "default" in info:
                raise ParseError(
                    "required parameters should not have defaults: '%s'"
                                     % line, lineno)
        elif tag == "prop":
            if "datatype" not in info:
                raise ParseError("@prop lines must include {type}: '%s'" %
                                  line, lineno)
            if "name" not in info:
                raise ParseError("@prop lines must provide a name: '%s'" %
                                  line, lineno)

    def _parseTypeLine(self, line, lineno):
        # handle these things:
        #    @method
        #    @returns description
        #    @returns {string} description
        #    @param NAME {type} description
        #    @param NAME
        #    @prop NAME {type} description
        #    @prop NAME
        # returns:
        #    tag: type of api element
        #    info: linenumber, required, default, name, datatype
        #    description

        info = {"line_number": lineno}
        line = line.rstrip("\n")
        pieces = line.split()

        if not pieces:
            raise ParseError("line is too short: '%s'" % line, lineno)
        if not pieces[0].startswith("@"):
            raise ParseError("type line should start with @: '%s'" % line,
                             lineno)
        tag = pieces[0][1:]
        skip = 1

        expect_name = tag in ("param", "prop")

        if len(pieces) == 1:
            description = ""
        else:
            if pieces[1].startswith("{"):
                # NAME is missing, pieces[1] is TYPE
                pass
            else:
                if expect_name:
                    info["required"] = not pieces[1].startswith("[")
                    name = pieces[1].strip("[ ]")
                    if "=" in name:
                        name, info["default"] = name.split("=")
                    info["name"] = name
                    skip += 1

            if len(pieces) > skip and pieces[skip].startswith("{"):
                info["datatype"] = pieces[skip].strip("{ }")
                skip += 1

            # we've got the metadata, now extract the description
            pieces = line.split(None, skip)
            if len(pieces) > skip:
                description = pieces[skip]
            else:
                description = ""
        self._validate_info(tag, info, line, lineno)
        return tag, info, description

def parse_hunks(text):
    # return a list of tuples. Each is one of:
    #    ("raw", string)         : non-API blocks
    #    ("api-json", dict)  : API blocks
    yield ("version", VERSION)
    lines = text.splitlines(True)
    line_number = 0
    markdown_string = ""
    while line_number < len(lines):
        line = lines[line_number]
        if line.startswith("<api"):
            if len(markdown_string) > 0:
                yield ("markdown", markdown_string)
                markdown_string = ""
            api, line_number = APIParser().parse(lines, line_number)
            # this business with 'leftover' is a horrible thing to do,
            # and exists only to collect the \n after the closing /api tag.
            # It's not needed probably, except to help keep compatibility
            # with the previous behaviour
            leftover = lines[line_number].lstrip("</api>")
            if len(leftover) > 0:
                markdown_string += leftover
            line_number = line_number + 1
            yield ("api-json", api)
        else:
            markdown_string += line
            line_number = line_number + 1
    if len(markdown_string) > 0:
        yield ("markdown", markdown_string)

class TestRenderer:
    # render docs for test purposes

    def getm(self, d, key):
        return d.get(key, "<MISSING>")

    def join_lines(self, text):
        return " ".join([line.strip() for line in text.split("\n")])

    def render_prop(self, p):
        s = "props[%s]: " % self.getm(p, "name")
        pieces = []
        for k in ("type", "description", "required", "default"):
            if k in p:
                pieces.append("%s=%s" % (k, self.join_lines(str(p[k]))))
        return s + ", ".join(pieces)

    def render_param(self, p):
        pieces = []
        for k in ("name", "type", "description", "required", "default"):
            if k in p:
                pieces.append("%s=%s" % (k, self.join_lines(str(p[k]))))
        yield ", ".join(pieces)
        for prop in p.get("props", []):
            yield " " + self.render_prop(prop)

    def render_method(self, method):
        yield "name= %s" % self.getm(method, "name")
        yield "type= %s" % self.getm(method, "type")
        yield "description= %s" % self.getm(method, "description")
        signature = method.get("signature")
        if signature:
            yield "signature= %s" % self.getm(method, "signature")
        params = method.get("params", [])
        if params:
            yield "parameters:"
            for p in params:
                for pline in self.render_param(p):
                    yield " " + pline
        r = method.get("returns", None)
        if r:
            yield "returns:"
            if "type" in r:
                yield " type= %s" % r["type"]
            if "description" in r:
                yield " description= %s" % self.join_lines(r["description"])
            props = r.get("props", [])
            for p in props:
                yield "  " + self.render_prop(p)

    def format_api(self, api):
        for mline in self.render_method(api):
            yield mline
        constructors = api.get("constructors", [])
        if constructors:
            yield "constructors:"
            for m in constructors:
                for mline in self.render_method(m):
                    yield " " + mline
        methods = api.get("methods", [])
        if methods:
            yield "methods:"
            for m in methods:
                for mline in self.render_method(m):
                    yield " " + mline
        properties = api.get("properties", [])
        if properties:
            yield "properties:"
            for p in properties:
                yield "  " + self.render_prop(p)

    def render_docs(self, docs_json, outf=sys.stdout):

        for (t,data) in docs_json:
            if t == "api-json":
                for line in self.format_api(data):
                    line = line.rstrip("\n")
                    outf.write("API: " + line + "\n")
            else:
                for line in str(data).split("\n"):
                    outf.write("MD :" +  line + "\n")

def hunks_to_dict(docs_json):
    exports = {}
    for (t,data) in docs_json:
        if t != "api-json":
            continue
        if data["name"]:
            exports[data["name"]] = data
    return exports

if __name__ == "__main__":
    json = False
    if sys.argv[1] == "--json":
        json = True
        del sys.argv[1]
    docs_text = open(sys.argv[1]).read()
    docs_parsed = list(parse_hunks(docs_text))
    if json:
        import simplejson
        print simplejson.dumps(docs_parsed, indent=2)
    else:
        TestRenderer().render_docs(docs_parsed)
