
#include <assert.h>

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


void XMLParser::syntaxError(const char *msg, uint backUp)
{
	mReader.unget(backUp);
    mReader.error(Exception::syntaxError, widenCString(msg), mReader.getPos());
}

void XMLParser::parseName(String &id)
{
    char16 ch = mReader.peek();
	CharInfo chi(ch);
    if (isAlpha(chi)) {
        mReader.beginRecording(id);
        mReader.recordChar(mReader.get());
        while (true) {
            ch = mReader.peek();
		    CharInfo chi(ch);
            if (isAlphanumeric(chi))
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
    char16 ch = mReader.peek();
    CharInfo chi(ch);
    while (isSpace(chi)) {
        ch = mReader.get();
		if (isLineBreak(ch))
			mReader.beginLine();
        chi = CharInfo(mReader.peek());
    }
}

void XMLParser::parseAttrValue(String &val)
{
    char16 ch = mReader.get();
    if (ch != '\"')
        syntaxError("'\"' expected");
    else {
        mReader.beginRecording(val);
        while (true) {
            ch = mReader.get();
            if (mReader.getEof(ch))
                syntaxError("Unterminated value");
            if (ch == '\"')
                break;
            mReader.recordChar(ch);
        }
        mReader.endRecording();
    }
}

XMLTag *XMLParser::parseTag()
{
    char16 ch;
    XMLTag *tag = new XMLTag();
    ch = mReader.peek();
    if (ch == '/') {
        tag->setEndTag();
        mReader.get();
    }
    else {
        if (ch == '!') {
            mReader.get();
            if (mReader.peek() != '-')
                syntaxError("badly formed tag");
            mReader.get();
            if (mReader.peek() != '-')
                syntaxError("badly formed tag");
            mReader.get();
            while (true) {
                ch = mReader.get();
                if (mReader.getEof(ch))
                    syntaxError("Unterminated comment tag");
                if (ch == '-') {
                    ch = mReader.get();
                    if (mReader.getEof(ch))
                        syntaxError("Unterminated comment tag");
                    if (ch != '-')
                        mReader.unget();
                    else {
                        ch = mReader.get();
                        if (mReader.getEof(ch))
                            syntaxError("Unterminated comment tag");
                        if (ch != '>')
                            syntaxError("encountered '--' in comment");
                        break;
                    }                
                }
		        if (isLineBreak(ch))
			        mReader.beginLine();
            }
            tag->setComment();
            return tag;
        }
    }
    parseName(tag->name());
    parseWhiteSpace();
    CharInfo chi(mReader.peek());
    while (isAlpha(chi)) {
        String attrValue;
        String attrName;
        parseName(attrName);
        if (mReader.peek() == '=') {
            mReader.get();
            parseAttrValue(attrValue);
        }
        tag->addAttribute(attrName, attrValue);
        parseWhiteSpace();
        chi = CharInfo(mReader.peek());
    }
    ch = mReader.get(); 
    if (mReader.getEof(ch))
        syntaxError("Unterminated tag");
    if (ch == '/') {
        tag->setEmpty();
        ch = mReader.get();
    }
    if (ch != '>')
        syntaxError("'>' expected");
    return tag;
}

void XMLParser::parseTagBody(XMLNode *parent, XMLTag *startTag)
{
    while (true) {
        char16 ch = mReader.get();
        while (ch != '<') {
            if (mReader.getEof(ch))
                syntaxError("Unterminated tag body");
            parent->addToBody(ch);
            ch = mReader.get();
        }
        XMLTag *tag = parseTag();
        if (tag->isEndTag() && (tag->name().compare(startTag->name()) == 0))
            break;
        else {
            XMLNode *child = new XMLNode(parent, tag);
            if (!tag->isEmpty())
                parseTagBody(child, tag);
        }
    }
}


XMLNode *XMLParser::parseDocument()
{
    XMLNode *top = new XMLNode(NULL, new XMLTag(widenCString("TOP")));
    char16 ch = mReader.get();
    while (ch == '<') {
        XMLTag *tag = parseTag();
        XMLNode *n = new XMLNode(top, tag);
        if (!tag->isEmpty()) {
            parseTagBody(n, tag);
        }
        parseWhiteSpace();
        ch = mReader.get();
        if (mReader.getEof(ch))
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
