class JS2StringElement {
public:

	JS2StringElement(uint16 length) : length(length), chars(new jschar[length]) { }

	JS2StringElement(uint16 length, jschar *source) : length(length), chars(new jschar[length]) { for (uint16 i = 0; i < length; i++) chars[i] = source[i]; }

	uint16 getLength() const { return length; }
	jschar *getChars() const { return chars; }


	void insert(uint16 index, uint16 length, jschar *text)
	{
		for (uint16 i = 0; i < length; i++, index++)
			chars[index] = text[i];
	}

private:
	uint16 length;
	jschar *chars;

};


class JS2String {
public:

	JS2String(uint16 length, jschar *text) : count(1), contents[0] = new JS2StringElement(length, text) { }


	JS2String(const JS2String &a, const JS2String &b) 
	{
		count = a.getCount() + b.getCount();
		if (count <= 4) {
			uint16 i;
			for (i = 0; i < a.getCount(); i++)
				contents[i] = a.contents[i];
			for (uint16 j = 0; j < b.getCount(); j++, i++)
				contents[i] = b.contents[j];
		}
		else {
			count = 1;
			contents[0] = new JS2StringElement(a.getLength() + b.getLength());
			uint16 i;
			uint16 index = 0;
			for (i = 0; i < a.getCount(); i++) {
				contents[0]->insert(index, a.contents[i]->getLength(), a.contents[i]->getChars());
				index += a.contents[i]->getLength();
			}
			for (i = 0; i < b.getCount(); i++) {
				contents[0]->insert(index, b.contents[i]->getLength(), b.contents[i]->getChars());
				index += b.contents[i]->getLength()
			}
		}
	}

	void add(const JS2String &a)
	{
		ASSERT((count + a.count) <= 4);
		for (uint16 i = 0; i < a.count; i++, count++)
			contents[count] = a.contents[i];
	}

	uint16 getCount() const 	{ return count; }

	uint16 getLength() const 	{ uint16 t = 0; for (uint16 i = 0; i < count; i++) t += contents[i]->getLength(); return t; }

	void collapse()		
	{ 
		if (count > 1) {
			JS2StringElement *t = new JS2StringElement(getLength());
			for (uint16 i = 0; i < count; i++) {
				t->insert(index, contents[i]->getLength(), contents[i]->getChars());
				index += contents[i]->getLength();
			}
			count = 1;
			contents[0] = t;
		}
	}

	jschar *getChars()	{ collapse(); return contents[0]->getChars(); }

private:
	uint16 count;
	JS2StringElement *contents[4];

};


JS2String &operator +(const JS2String &a, const JS2String &b) { return JS2String(a, b); }
JS2String &operator +=(const JS2String &a, const JS2String &b)
{
	if ((a.getCount() + b.getCount) <= 4) {
		a.add(b);
		return a;
	}
	else
		return JS2String(a, b);
}
