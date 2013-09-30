

(function () {
    var tokenise = function (str) {
        var tokens = []
        ,   re = {
                "float":        /^-?(([0-9]+\.[0-9]*|[0-9]*\.[0-9]+)([Ee][-+]?[0-9]+)?|[0-9]+[Ee][-+]?[0-9]+)/
            ,   "integer":      /^-?(0([Xx][0-9A-Fa-f]+|[0-7]*)|[1-9][0-9]*)/
            ,   "identifier":   /^[A-Z_a-z][0-9A-Z_a-z]*/
            ,   "string":       /^"[^"]*"/
            ,   "whitespace":   /^(?:[\t\n\r ]+|[\t\n\r ]*((\/\/.*|\/\*(.|\n|\r)*?\*\/)[\t\n\r ]*))+/
            ,   "other":        /^[^\t\n\r 0-9A-Z_a-z]/
            }
        ,   types = []
        ;
        for (var k in re) types.push(k);
        while (str.length > 0) {
            var matched = false;
            for (var i = 0, n = types.length; i < n; i++) {
                var type = types[i];
                str = str.replace(re[type], function (tok) {
                    tokens.push({ type: type, value: tok });
                    matched = true;
                    return "";
                });
                if (matched) break;
            }
            if (matched) continue;
            throw new Error("Token stream not progressing");
        }
        return tokens;
    };
    
    var parse = function (tokens) {
        var line = 1;
        tokens = tokens.slice();
        
        var FLOAT = "float"
        ,   INT = "integer"
        ,   ID = "identifier"
        ,   STR = "string"
        ,   OTHER = "other"
        ;
        
        var WebIDLParseError = function (str, line, input, tokens) {
            this.message = str;
            this.line = line;
            this.input = input;
            this.tokens = tokens;
        };
        WebIDLParseError.prototype.toString = function () {
            return this.message + ", line " + this.line + " (tokens: '" + this.input + "')\n" +
                   JSON.stringify(this.tokens, null, 4);
        };
        
        var error = function (str) {
            var tok = "", numTokens = 0, maxTokens = 5;
            while (numTokens < maxTokens && tokens.length > numTokens) {
                tok += tokens[numTokens].value;
                numTokens++;
            }
            throw new WebIDLParseError(str, line, tok, tokens.slice(0, 5));
        };
        
        var last_token = null;
        
        var consume = function (type, value) {
            if (!tokens.length || tokens[0].type !== type) return;
            if (typeof value === "undefined" || tokens[0].value === value) {
                 last_token = tokens.shift();
                 if (type === ID) last_token.value = last_token.value.replace(/^_/, "");
                 return last_token;
             }
        };
        
        var ws = function () {
            if (!tokens.length) return;
            if (tokens[0].type === "whitespace") {
                var t = tokens.shift();
                t.value.replace(/\n/g, function (m) { line++; return m; });
                return t;
            }
        };
        
        var all_ws = function () {
            var t = { type: "whitespace", value: "" };
            while (true) {
                var w = ws();
                if (!w) break;
                t.value += w.value;
            }
            if (t.value.length > 0) return t;
        };
        
        var integer_type = function () {
            var ret = "";
            all_ws();
            if (consume(ID, "unsigned")) ret = "unsigned ";
            all_ws();
            if (consume(ID, "short")) return ret + "short";
            if (consume(ID, "long")) {
                ret += "long";
                all_ws();
                if (consume(ID, "long")) return ret + " long";
                return ret;
            }
            if (ret) error("Failed to parse integer type");
        };
        
        var float_type = function () {
            var ret = "";
            all_ws();
            if (consume(ID, "unrestricted")) ret = "unrestricted ";
            all_ws();
            if (consume(ID, "float")) return ret + "float";
            if (consume(ID, "double")) return ret + "double";
            if (ret) error("Failed to parse float type");
        };
        
        var primitive_type = function () {
            var num_type = integer_type() || float_type();
            if (num_type) return num_type;
            all_ws();
            if (consume(ID, "boolean")) return "boolean";
            if (consume(ID, "byte")) return "byte";
            if (consume(ID, "octet")) return "octet";
        };
        
        var const_value = function () {
            if (consume(ID, "true")) return { type: "boolean", value: true };
            if (consume(ID, "false")) return { type: "boolean", value: false };
            if (consume(ID, "null")) return { type: "null" };
            if (consume(ID, "Infinity")) return { type: "Infinity", negative: false };
            if (consume(ID, "NaN")) return { type: "NaN" };
            var ret = consume(FLOAT) || consume(INT);
            if (ret) return { type: "number", value: 1 * ret.value };
            var tok = consume(OTHER, "-");
            if (tok) {
                if (consume(ID, "Infinity")) return { type: "Infinity", negative: true };
                else tokens.unshift(tok);
            }
        };
        
        var type_suffix = function (obj) {
            while (true) {
                all_ws();
                if (consume(OTHER, "?")) {
                    if (obj.nullable) error("Can't nullable more than once");
                    obj.nullable = true;
                }
                else if (consume(OTHER, "[")) {
                    all_ws();
                    consume(OTHER, "]") || error("Unterminated array type");
                    if (!obj.array) obj.array = 1;
                    else obj.array++;
                }
                else return;
            }
        };
        
        var single_type = function () {
            var prim = primitive_type()
            ,   ret = { sequence: false, nullable: false, array: false, union: false }
            ;
            if (prim) {
                ret.idlType = prim;
            }
            else if (consume(ID, "sequence")) {
                all_ws();
                if (!consume(OTHER, "<")) {
                    ret.idlType = "sequence";
                }
                else {
                    ret.sequence = true;
                    ret.idlType = type() || error("Error parsing sequence type");
                    all_ws();
                    if (!consume(OTHER, ">")) error("Unterminated sequence");
                    all_ws();
                    if (consume(OTHER, "?")) ret.nullable = true;
                    return ret;
                }
            }
            else {
                var name = consume(ID);
                if (!name) return;
                ret.idlType = name.value;
            }
            type_suffix(ret);
            if (ret.nullable && ret.idlType === "any") error("Type any cannot be made nullable");
            return ret;
        };
        
        var union_type = function () {
            all_ws();
            if (!consume(OTHER, "(")) return;
            var ret = { sequence: false, nullable: false, array: false, union: true, idlType: [] };
            var fst = type() || error("Union type with no content");
            ret.idlType.push(fst);
            while (true) {
                all_ws();
                if (!consume(ID, "or")) break;
                var typ = type() || error("No type after 'or' in union type");
                ret.idlType.push(typ);
            }
            if (!consume(OTHER, ")")) error("Unterminated union type");
            type_suffix(ret);
            return ret;
        };
        
        var type = function () {
            return single_type() || union_type();
        };
        
        var argument = function () {
            var ret = { optional: false, variadic: false };
            ret.extAttrs = extended_attrs();
            all_ws();
            if (consume(ID, "optional")) {
                ret.optional = true;
                all_ws();
            }
            ret.idlType = type();
            if (!ret.idlType) return;
            if (!ret.optional) {
                all_ws();
                if (tokens.length >= 3 &&
                    tokens[0].type === "other" && tokens[0].value === "." &&
                    tokens[1].type === "other" && tokens[1].value === "." &&
                    tokens[2].type === "other" && tokens[2].value === "."
                    ) {
                    tokens.shift();
                    tokens.shift();
                    tokens.shift();
                    ret.variadic = true;
                }
            }
            all_ws();
            var name = consume(ID) || error("No name in argument");
            ret.name = name.value;
            if (ret.optional) {
                all_ws();
                ret["default"] = default_();
            }
            return ret;
        };
        
        var argument_list = function () {
            var arg = argument(), ret = [];
            if (!arg) return ret;
            ret.push(arg);
            while (true) {
                all_ws();
                if (!consume(OTHER, ",")) return ret;
                all_ws();
                var nxt = argument() || error("Trailing comma in arguments list");
                ret.push(nxt);
            }
        };
        
        var simple_extended_attr = function () {
            all_ws();
            var name = consume(ID);
            if (!name) return;
            var ret = {
                name: name.value
            ,   "arguments": null
            };
            all_ws();
            var eq = consume(OTHER, "=");
            if (eq) {
                all_ws();
                ret.rhs = consume(ID);
                if (!ret.rhs) return error("No right hand side to extended attribute assignment");
            }
            all_ws();
            if (consume(OTHER, "(")) {
                ret["arguments"] = argument_list();
                all_ws();
                consume(OTHER, ")") || error("Unclosed argument in extended attribute");
            }
            return ret;
        };
        
        // Note: we parse something simpler than the official syntax. It's all that ever
        // seems to be used
        var extended_attrs = function () {
            var eas = [];
            all_ws();
            if (!consume(OTHER, "[")) return eas;
            eas[0] = simple_extended_attr() || error("Extended attribute with not content");
            all_ws();
            while (consume(OTHER, ",")) {
                all_ws();
                eas.push(simple_extended_attr() || error("Trailing comma in extended attribute"));
                all_ws();
            }
            consume(OTHER, "]") || error("No end of extended attribute");
            return eas;
        };
        
        var default_ = function () {
            all_ws();
            if (consume(OTHER, "=")) {
                all_ws();
                var def = const_value();
                if (def) {
                    return def;
                }
                else {
                    var str = consume(STR) || error("No value for default");
                    str.value = str.value.replace(/^"/, "").replace(/"$/, "");
                    return str;
                }
            }
        };
        
        var const_ = function () {
            all_ws();
            if (!consume(ID, "const")) return;
            var ret = { type: "const", nullable: false };
            all_ws();
            var typ = primitive_type();
            if (!typ) {
                typ = consume(ID) || error("No type for const");
                typ = typ.value;
            }
            ret.idlType = typ;
            all_ws();
            if (consume(OTHER, "?")) {
                ret.nullable = true;
                all_ws();
            }
            var name = consume(ID) || error("No name for const");
            ret.name = name.value;
            all_ws();
            consume(OTHER, "=") || error("No value assignment for const");
            all_ws();
            var cnt = const_value();
            if (cnt) ret.value = cnt;
            else error("No value for const");
            all_ws();
            consume(OTHER, ";") || error("Unterminated const");
            return ret;
        };
        
        var inheritance = function () {
            all_ws();
            if (consume(OTHER, ":")) {
                all_ws();
                var inh = consume(ID) || error ("No type in inheritance");
                return inh.value;
            }
        };
        
        var operation_rest = function (ret) {
            all_ws();
            if (!ret) ret = {};
            var name = consume(ID);
            ret.name = name ? name.value : null;
            all_ws();
            consume(OTHER, "(") || error("Invalid operation");
            ret["arguments"] = argument_list();
            all_ws();
            consume(OTHER, ")") || error("Unterminated operation");
            all_ws();
            consume(OTHER, ";") || error("Unterminated operation");
            return ret;
        };
        
        var callback = function () {
            all_ws();
            var ret;
            if (!consume(ID, "callback")) return;
            all_ws();
            var tok = consume(ID, "interface");
            if (tok) {
                tokens.unshift(tok);
                ret = interface_();
                ret.type = "callback interface";
                return ret;
            }
            var name = consume(ID) || error("No name for callback");
            ret = { type: "callback", name: name.value };
            all_ws();
            consume(OTHER, "=") || error("No assignment in callback");
            all_ws();
            ret.idlType = return_type();
            all_ws();
            consume(OTHER, "(") || error("No arguments in callback");
            ret["arguments"] = argument_list();
            all_ws();
            consume(OTHER, ")") || error("Unterminated callback");
            all_ws();
            consume(OTHER, ";") || error("Unterminated callback");
            return ret;
        };

        var attribute = function () {
            all_ws();
            var grabbed = []
            ,   ret = {
                type:           "attribute"
            ,   "static":       false
            ,   stringifier:    false
            ,   inherit:        false
            ,   readonly:       false
            };
            if (consume(ID, "static")) {
                ret["static"] = true;
                grabbed.push(last_token);
            }
            else if (consume(ID, "stringifier")) {
                ret.stringifier = true;
                grabbed.push(last_token);
            }
            var w = all_ws();
            if (w) grabbed.push(w);
            if (consume(ID, "inherit")) {
                if (ret["static"] || ret.stringifier) error("Cannot have a static or stringifier inherit");
                ret.inherit = true;
                grabbed.push(last_token);
                var w = all_ws();
                if (w) grabbed.push(w);
            }
            if (consume(ID, "readonly")) {
                ret.readonly = true;
                grabbed.push(last_token);
                var w = all_ws();
                if (w) grabbed.push(w);
            }
            if (!consume(ID, "attribute")) {
                tokens = grabbed.concat(tokens);
                return;
            }
            all_ws();
            ret.idlType = type() || error("No type in attribute");
            if (ret.idlType.sequence) error("Attributes cannot accept sequence types");
            all_ws();
            var name = consume(ID) || error("No name in attribute");
            ret.name = name.value;
            all_ws();
            consume(OTHER, ";") || error("Unterminated attribute");
            return ret;
        };
        
        var return_type = function () {
            var typ = type();
            if (!typ) {
                if (consume(ID, "void")) {
                    return "void";
                }
                else error("No return type");
            }
            return typ;
        };
        
        var operation = function () {
            all_ws();
            var ret = {
                type:           "operation"
            ,   getter:         false
            ,   setter:         false
            ,   creator:        false
            ,   deleter:        false
            ,   legacycaller:   false
            ,   "static":       false
            ,   stringifier:    false
            };
            while (true) {
                all_ws();
                if (consume(ID, "getter")) ret.getter = true;
                else if (consume(ID, "setter")) ret.setter = true;
                else if (consume(ID, "creator")) ret.creator = true;
                else if (consume(ID, "deleter")) ret.deleter = true;
                else if (consume(ID, "legacycaller")) ret.legacycaller = true;
                else break;
            }
            if (ret.getter || ret.setter || ret.creator || ret.deleter || ret.legacycaller) {
                all_ws();
                ret.idlType = return_type();
                operation_rest(ret);
                return ret;
            }
            if (consume(ID, "static")) {
                ret["static"] = true;
                ret.idlType = return_type();
                operation_rest(ret);
                return ret;
            }
            else if (consume(ID, "stringifier")) {
                ret.stringifier = true;
                all_ws();
                if (consume(OTHER, ";")) return ret;
                ret.idlType = return_type();
                operation_rest(ret);
                return ret;
            }
            ret.idlType = return_type();
            all_ws();
            if (consume(ID, "iterator")) {
                all_ws();
                ret.type = "iterator";
                if (consume(ID, "object")) {
                    ret.iteratorObject = "object";
                }
                else if (consume(OTHER, "=")) {
                    all_ws();
                    var name = consume(ID) || error("No right hand side in iterator");
                    ret.iteratorObject = name.value;
                }
                all_ws();
                consume(OTHER, ";") || error("Unterminated iterator");
                return ret;
            }
            else {
                operation_rest(ret);
                return ret;
            }
        };
        
        var identifiers = function (arr) {
            while (true) {
                all_ws();
                if (consume(OTHER, ",")) {
                    all_ws();
                    var name = consume(ID) || error("Trailing comma in identifiers list");
                    arr.push(name.value);
                }
                else break;
            }
        };
        
        var serialiser = function () {
            all_ws();
            if (!consume(ID, "serializer")) return;
            var ret = { type: "serializer" };
            all_ws();
            if (consume(OTHER, "=")) {
                all_ws();
                if (consume(OTHER, "{")) {
                    ret.patternMap = true;
                    all_ws();
                    var id = consume(ID);
                    if (id && id.value === "getter") {
                        ret.names = ["getter"];
                    }
                    else if (id && id.value === "inherit") {
                        ret.names = ["inherit"];
                        identifiers(ret.names);
                    }
                    else if (id) {
                        ret.names = [id.value];
                        identifiers(ret.names);
                    }
                    else {
                        ret.names = [];
                    }
                    all_ws();
                    consume(OTHER, "}") || error("Unterminated serializer pattern map");
                }
                else if (consume(OTHER, "[")) {
                    ret.patternList = true;
                    all_ws();
                    var id = consume(ID);
                    if (id && id.value === "getter") {
                        ret.names = ["getter"];
                    }
                    else if (id) {
                        ret.names = [id.value];
                        identifiers(ret.names);
                    }
                    else {
                        ret.names = [];
                    }
                    all_ws();
                    consume(OTHER, "]") || error("Unterminated serializer pattern list");
                }
                else {
                    var name = consume(ID) || error("Invalid serializer");
                    ret.name = name.value;
                }
                all_ws();
                consume(OTHER, ";") || error("Unterminated serializer");
                return ret;
            }
            else if (consume(OTHER, ";")) {
                // noop, just parsing
            }
            else {
                ret.idlType = return_type();
                all_ws();
                ret.operation = operation_rest();
            }
            return ret;
        };
        
        var interface_ = function (isPartial) {
            all_ws();
            if (!consume(ID, "interface")) return;
            all_ws();
            var name = consume(ID) || error("No name for interface");
            var ret = {
                type:   "interface"
            ,   name:   name.value
            ,   partial:    false
            ,   members:    []
            };
            if (!isPartial) ret.inheritance = inheritance() || null;
            all_ws();
            consume(OTHER, "{") || error("Bodyless interface");
            while (true) {
                all_ws();
                if (consume(OTHER, "}")) {
                    all_ws();
                    consume(OTHER, ";") || error("Missing semicolon after interface");
                    return ret;
                }
                var ea = extended_attrs();
                all_ws();
                var cnt = const_();
                if (cnt) {
                    cnt.extAttrs = ea;
                    ret.members.push(cnt);
                    continue;
                }
                var mem = serialiser() || attribute() || operation() || error("Unknown member");
                mem.extAttrs = ea;
                ret.members.push(mem);
            }
        };
        
        var partial = function () {
            all_ws();
            if (!consume(ID, "partial")) return;
            var thing = dictionary(true) || interface_(true) || error("Partial doesn't apply to anything");
            thing.partial = true;
            return thing;
        };
        
        var dictionary = function (isPartial) {
            all_ws();
            if (!consume(ID, "dictionary")) return;
            all_ws();
            var name = consume(ID) || error("No name for dictionary");
            var ret = {
                type:   "dictionary"
            ,   name:   name.value
            ,   partial:    false
            ,   members:    []
            };
            if (!isPartial) ret.inheritance = inheritance() || null;
            all_ws();
            consume(OTHER, "{") || error("Bodyless dictionary");
            while (true) {
                all_ws();
                if (consume(OTHER, "}")) {
                    all_ws();
                    consume(OTHER, ";") || error("Missing semicolon after dictionary");
                    return ret;
                }
                var ea = extended_attrs();
                all_ws();
                var typ = type() || error("No type for dictionary member");
                all_ws();
                var name = consume(ID) || error("No name for dictionary member");
                ret.members.push({
                    type:       "field"
                ,   name:       name.value
                ,   idlType:    typ
                ,   extAttrs:   ea
                ,   "default":  default_()
                });
                all_ws();
                consume(OTHER, ";") || error("Unterminated dictionary member");
            }
        };
        
        var exception = function () {
            all_ws();
            if (!consume(ID, "exception")) return;
            all_ws();
            var name = consume(ID) || error("No name for exception");
            var ret = {
                type:   "exception"
            ,   name:   name.value
            ,   members:    []
            };
            ret.inheritance = inheritance() || null;
            all_ws();
            consume(OTHER, "{") || error("Bodyless exception");
            while (true) {
                all_ws();
                if (consume(OTHER, "}")) {
                    all_ws();
                    consume(OTHER, ";") || error("Missing semicolon after exception");
                    return ret;
                }
                var ea = extended_attrs();
                all_ws();
                var cnt = const_();
                if (cnt) {
                    cnt.extAttrs = ea;
                    ret.members.push(cnt);
                }
                else {
                    var typ = type();
                    all_ws();
                    var name = consume(ID);
                    all_ws();
                    if (!typ || !name || !consume(OTHER, ";")) error("Unknown member in exception body");
                    ret.members.push({
                        type:       "field"
                    ,   name:       name.value
                    ,   idlType:    typ
                    ,   extAttrs:   ea
                    });
                }
            }
        };
        
        var enum_ = function () {
            all_ws();
            if (!consume(ID, "enum")) return;
            all_ws();
            var name = consume(ID) || error("No name for enum");
            var ret = {
                type:   "enum"
            ,   name:   name.value
            ,   values: []
            };
            all_ws();
            consume(OTHER, "{") || error("No curly for enum");
            var saw_comma = false;
            while (true) {
                all_ws();
                if (consume(OTHER, "}")) {
                    all_ws();
                    if (saw_comma) error("Trailing comma in enum");
                    consume(OTHER, ";") || error("No semicolon after enum");
                    return ret;
                }
                var val = consume(STR) || error("Unexpected value in enum");
                ret.values.push(val.value.replace(/"/g, ""));
                all_ws();
                if (consume(OTHER, ",")) {
                    all_ws();
                    saw_comma = true;
                }
                else {
                    saw_comma = false;
                }
            }
        };
        
        var typedef = function () {
            all_ws();
            if (!consume(ID, "typedef")) return;
            var ret = {
                type:   "typedef"
            };
            all_ws();
            ret.typeExtAttrs = extended_attrs();
            all_ws();
            ret.idlType = type() || error("No type in typedef");
            all_ws();
            var name = consume(ID) || error("No name in typedef");
            ret.name = name.value;
            all_ws();
            consume(OTHER, ";") || error("Unterminated typedef");
            return ret;
        };
        
        var implements_ = function () {
            all_ws();
            var target = consume(ID);
            if (!target) return;
            var w = all_ws();
            if (consume(ID, "implements")) {
                var ret = {
                    type:   "implements"
                ,   target: target.value
                };
                all_ws();
                var imp = consume(ID) || error("Incomplete implements statement");
                ret["implements"] = imp.value;
                all_ws();
                consume(OTHER, ";") || error("No terminating ; for implements statement");
                return ret;
            }
            else {
                // rollback
                tokens.unshift(w);
                tokens.unshift(target);
            }
        };
        
        var definition = function () {
            return  callback()      ||
                    interface_()    ||
                    partial()       ||
                    dictionary()    ||
                    exception()     ||
                    enum_()         ||
                    typedef()       ||
                    implements_()
                    ;
        };
        
        var definitions = function () {
            if (!tokens.length) return [];
            var defs = [];
            while (true) {
                var ea = extended_attrs()
                ,   def = definition();
                if (!def) {
                    if (ea.length) error("Stray extended attributes");
                    break;
                }
                def.extAttrs = ea;
                defs.push(def);
            }
            return defs;
        };
        var res = definitions();
        if (tokens.length) error("Unrecognised tokens");
        return res;
    };

    var obj = {
        parse:  function (str) {
            var tokens = tokenise(str);
            return parse(tokens);
        }
    };
    if (typeof module !== "undefined" && module.exports) {
        module.exports = obj;
    }
    else {
        window.WebIDL2 = obj;
    }
}());
