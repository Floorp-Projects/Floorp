
#include <assert.h>
#include <stdio.h>

#include "parser.h"
#include "world.h"
#include "xmlparser.h"

namespace JavaScript {
    

bool XMLTag::getValue(const String &name, String &value)
{
    AttributeList::iterator i = mAttributeList.find(name);
    if (i == mAttributeList.end())
        return false;
    else {
        value = i->second;
        return true;
    }
}

/* ---------------------------------------------------------------------- */


XMLLexer::XMLLexer(const char *filename)
{
    base = NULL;
    FILE *f = fopen(filename, "r");
    if (f) {
        fseek(f, 0, SEEK_END);
        long position = ftell(f);
        if (position > 0) {
            uint32 length = uint32(position);
            base = new char[length + 1];
            fseek(f, 0, SEEK_SET);
            fread(base, 1, length, f);
            fclose(f);
            base[length] = '\0';
            p = base;
            end = base + length;
        }
    }
}

void XMLLexer::beginRecording(String &s)
{
	recordString = &s;
	recordBase = p;
	recordPos = p;
}

void XMLLexer::recordChar(char c)
{
	ASSERT(recordString);
	if (recordPos) {
		if (recordPos != end && *recordPos == c) {
			recordPos++;
			return;
		} else {
			insertChars(*recordString, 0, recordBase, uint32(recordPos - recordBase));
			recordPos = NULL;
		}
	}
	*recordString += c;
}

String &XMLLexer::endRecording()
{
	String *rs = recordString;
	ASSERT(rs);
	if (recordPos)
		insertChars(*rs, 0, recordBase, uint32(recordPos - recordBase));
	recordString = NULL;
	return *rs;
}

/* ---------------------------------------------------------------------- */


void XMLParser::syntaxError(const char *msg, uint backUp)
{
	mReader.unget(backUp);
    throw Exception(Exception::syntaxError, widenCString(msg));
}

void XMLParser::parseName(String &id)
{
    char ch = mReader.peek();
    if (mReader.isAlpha(ch)) {
        mReader.beginRecording(id);
        mReader.recordChar(mReader.get());
        while (true) {
            ch = mReader.peek();
            if (mReader.isAlphanumeric(ch))
		        mReader.recordChar(mReader.get());
            else
                break;        
        }
        mReader.endRecording();
    }
    else
        syntaxError("Invalid name");
}

void XMLParser::parseWhiteSpace()
{
    while (mReader.isSpace(mReader.peek())) {
        char ch = mReader.get();
		if (mReader.isLineBreak(ch))
			mReader.beginLine();
    }
}

void XMLParser::parseAttrValue(String &val)
{
    char ch = mReader.get();
    if (ch != '\"')
        syntaxError("'\"' expected");
    else {
        mReader.beginRecording(val);
        while (true) {
            ch = mReader.get();
            if (mReader.getEof())
                syntaxError("Unterminated value");
            if (ch == '\"')
                break;
            mReader.recordChar(ch);
        }
        mReader.endRecording();
    }
}

void XMLParser::parseStringLiteral(String &val)
{
    char quotech = mReader.get();
    if ((quotech != '\"') || (quotech != '\''))
        syntaxError("\" or \' expected");
    else {
        mReader.beginRecording(val);
        while (true) {
            char ch = mReader.get();
            if (mReader.getEof())
                syntaxError("Unterminated string literal");
            if (ch == quotech)
                break;
            mReader.recordChar(ch);
        }
        mReader.endRecording();
    }
}


XMLTag *XMLParser::parseTag()
{
    XMLTag *tag = new XMLTag();
    char ch = mReader.peek();
    if (ch == '/') {
        tag->setTag(EndTag);
        mReader.skip(1);
    }
    else {
        if (ch == '!') {
            mReader.skip(1);
            if (mReader.match("--", 2)) {
                mReader.skip(2);
                tag->setTag(CommentTag);
                while (true) {
                    ch = mReader.get();
                    if (mReader.getEof())
                        syntaxError("Unterminated comment tag");
                    if (ch == '-') {
                        if (mReader.match("->", 2)) {
                            mReader.skip(2);
                            return tag;
                        }
                        if (mReader.peek() == '-')
                            syntaxError("encountered '--' in comment");
                    }
                    else
		                if (mReader.isLineBreak(ch))
			                mReader.beginLine();
                }
            } else {
                if (mReader.match("[CDATA[", 7)) {
                    mReader.skip(7);
                    tag->setTag(CDataTag);
                    while (true) {
                        ch = mReader.get();
                        if (mReader.getEof())
                            syntaxError("Unterminated CDATA tag");
                        if (ch == ']') {
                            if (mReader.match("]>", 2)) {
                                mReader.skip(2);
                                return tag;
                            }
                        }
                        else
		                    if (mReader.isLineBreak(ch))
			                    mReader.beginLine();
                    }
                }
            }
        }
    }
    parseName(tag->name());
    parseWhiteSpace();
    while (isAlpha(mReader.peek())) {
        String attrValue;
        String attrName;
        parseName(attrName);
        if (mReader.peek() == '=') {
            mReader.get();
            parseAttrValue(attrValue);
        }
        tag->addAttribute(attrName, attrValue);
        parseWhiteSpace();
    }
    ch = mReader.get(); 
    if (mReader.getEof())
        syntaxError("Unterminated tag");
    if (ch == '/') {
        tag->setTag(EmptyTag);
        ch = mReader.get();
    }
    if (ch != '>')
        syntaxError("'>' expected");
    return tag;
}

void XMLParser::parseTagBody(XMLNode *parent, XMLTag *startTag)
{
    while (true) {
        char ch = mReader.get();
        while (ch != '<') {
            if (mReader.getEof())
                syntaxError("Unterminated tag body");
            parent->addToBody(ch);
            ch = mReader.get();
        }
        XMLTag *tag = parseTag();
        if (tag->isEndTag() && (tag->name().compare(startTag->name()) == 0))
            break;
        else {
            XMLNode *child = new XMLNode(parent, tag);
            if (tag->hasContent())
                parseTagBody(child, tag);
        }
    }
}


XMLNode *XMLParser::parseDocument()
{
    XMLNode *top = new XMLNode(NULL, new XMLTag(widenCString("TOP")));
    char ch = mReader.get();
    while (ch == '<') {
        XMLTag *tag = parseTag();
        XMLNode *n = new XMLNode(top, tag);
        if (!tag->isEmpty()) {
            parseTagBody(n, tag);
        }
        parseWhiteSpace();
        ch = mReader.get();
        if (mReader.getEof())
            break;
    }
    return top;
}

Formatter& operator<<(Formatter& f, const XMLTag& tag)
{
    tag.print(f);
    return f;
}

Formatter& operator<<(Formatter& f, const XMLNode& node)
{
    node.print(f);
    return f;
}

} /* namespace JavaScript */
