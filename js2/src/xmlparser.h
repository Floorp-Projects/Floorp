

#include <assert.h>

#include <map>

#include "parser.h"
#include "world.h"


namespace JavaScript {

typedef enum {Tag, EmptyTag, EndTag, CommentTag, DocTypeTag, ProcessInstructionTag } TagFlag;


class XMLLexer 
{
public:
    XMLLexer(const char *filename);
    ~XMLLexer() { if (base) delete base; }

    char get()                  { if (p != end) return *p++; else return '\0'; }
    void skip(uint32 n)         { p += n; ASSERT(p <= end); }
    char peek()                 { return *p; }
    void unget(uint32 n = 1)    { p -=n; ASSERT(p >= base); }
    bool getEof()               { return (p == end); }
    bool hasAvailable(uint32 n) { return (end - p) >= n; }
    bool match(const char *s, uint32 n)   { return hasAvailable(n) && (strstr(p, s) == p); }
    bool match(const char *s)             { return match(s, strlen(s)); }

    uint32 getPos()             { return p - base; }

    void beginRecording(String &s);
    String &endRecording();
    void recordChar(char c);

    void beginLine()            { }

    char *base;
    char *p;
    char *end;

    char *recordPos;
    char *recordBase;
    String *recordString; 

    bool isAlpha(char c)        { return JavaScript::isAlpha(CharInfo(c)); }
    bool isAlphanumeric(char c) { return JavaScript::isAlphanumeric(CharInfo(c)); }
    bool isSpace(char c)        { return JavaScript::isSpace(CharInfo(c)); }
    bool isLineBreak(char c)    { return JavaScript::isLineBreak(CharInfo(c)); }

};


class XMLNode;
typedef std::vector<XMLNode *> XMLNodeList;
typedef std::multimap<String, String, std::less<String> > AttributeList;
typedef AttributeList::value_type AttributeValue;

class XMLTag {
public:
    XMLTag() : mFlag(Tag) { }
    XMLTag(String name) : mName(name), mFlag(Tag)  { }
    
    void addAttribute(const String &name, const String &value)    { mAttributeList.insert(AttributeValue(name, value) ); }
    bool getValue(const String &name, String &value);
    bool hasAttribute(const String &name)                 { return (mAttributeList.find(name) != mAttributeList.end()); }

    String &name()      { return mName; }

    void setEmpty()     { mFlag = EmptyTag; }
    void setComment()   { mFlag = EmptyTag; }
    void setEndTag()    { mFlag = EndTag; }

    bool isEmpty() const      { return mFlag == EmptyTag; }
    bool isEndTag() const     { return mFlag == EndTag; }
    bool isComment() const    { return mFlag == CommentTag; }

    String mName;
    TagFlag mFlag;
    AttributeList mAttributeList;
    
    void print(Formatter& f) const
    {
        if (isComment())
            f << "XML Comment tag\n";
        else
            f << "XMLTag '" << mName << "'\n";
        for (AttributeList::const_iterator i = mAttributeList.begin(); i != mAttributeList.end(); i++) {
            f << "\t'" << i->first << "' = '" << i->second << "'\n";
        }
    }
};

Formatter& operator<<(Formatter& f, const XMLTag& tag);
Formatter& operator<<(Formatter& f, const XMLNode& node);

class XMLNode 
{
public:
    XMLNode(XMLNode *parent, XMLTag *tag) : mParent(parent), mTag(tag) { if (parent) parent->addChild(this); }

    String &name()                  { return mTag->name(); }
    XMLTag *tag()                   { return mTag; }

    String &body()                  { return mBody; }

    void addToBody(String contents) { mBody += contents; }
    void addToBody(char16 ch)       { mBody += ch; }

    void addChild(XMLNode *child)   { mChildren.push_back(child); }
    XMLNodeList &children()         { return mChildren; }

    bool getValue(const String &name, String &value)
                                    { return mTag->getValue(name, value); }
    bool hasAttribute(const String &name) { return mTag->hasAttribute(name); }


    void print(Formatter& f) const
    {
        f << *mTag;
        if (mParent)
            f << "parent = '" <<  mParent->name() << "'";
        else
            f << "parent = NULL";

        XMLNodeList::const_iterator i = mChildren.begin();
        XMLNodeList::const_iterator end = mChildren.end();
        if (i != end) {
            f << "\nChildren :\n";
            while (i != end) {
                f << **i;
                f << "\n";
                i++;
            }
        }
        f << "\n";
    }

    XMLNodeList mChildren;
    XMLNode *mParent;
    XMLTag *mTag;
    String mBody;	
};



class XMLParser {
public:
	void syntaxError(const char *message, uint backUp = 1);
    char16 doEscape();

    XMLParser(const char *fileName) : mReader(fileName) { }


    void parseName(String &id);
    void parseWhiteSpace();
    void parseAttrValue(String &val);
    XMLTag *parseTag();
    void parseTagBody(XMLNode *parent, XMLTag *startTag);
    XMLNode *parseDocument();


    XMLLexer mReader;

};



}   /* namespace JavaScript */
