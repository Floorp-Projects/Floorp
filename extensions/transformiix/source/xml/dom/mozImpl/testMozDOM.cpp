//Simple test program for the Mozilla DOM wrapper classes

#include "mozilladom.h"
#include "nsXMLDocument.h"
#include "iostream.h"

void main()
{
  /*
  Test code for Pure Mozilla Objects

  nsXMLDocument* nsxmlDocument = new nsXMLDocument();

  nsIDOMElement* nsElem1 = NULL;
  nsIDOMNode* returnElem = NULL;
  nsIDOMElement* docElem = NULL;
  MozillaString tag;

  nsxmlDocument->CreateElement("TestFirstElement", &nsElem1);
  nsxmlDocument->AppendChild(nsElem1, &returnElem);

  if (!returnElem)
    cout << "The AppendChild did not work" << endl; 

  nsElem1->GetTagName(tag.getNSString());

  cout << tag << endl; */
  

  nsXMLDocument* nsxmlDocument = new nsXMLDocument();
  Document* document = new Document(nsxmlDocument);
  Element* elem1;
  Element* subelem1;
  Element* subelem2;
  Attr* attr2;
  Attr* tmpAttr;
  NodeList* subElems = NULL;
  Element* tmpElem = NULL;
  NamedNodeMap* attrs = NULL;
  Node* tmpNode;
  
  elem1 = document->createElement("FirstElement");

  cout << "elem1 tag name=" << elem1->getTagName() << endl;
  
  elem1->setAttribute("Attr1", "Attr1Value");
  cout << "elem1 attr1=" << elem1->getAttribute("Attr1") << endl;

  attr2 = document->createAttribute("Attr2");
  attr2->setValue("attr2 value");
  cout << "attr2 name=" << attr2->getName() << endl;
  cout << "attr2 value=" << attr2->getValue() << endl;

  elem1->setAttributeNode(attr2);
  cout << "elem1 attr2=" << elem1->getAttribute("Attr2") << endl;

  tmpAttr = elem1->getAttributeNode("Attr2");
  cout << "tmpAttr name=" << tmpAttr->getName() << endl;
  cout << "tmpAttr value=" << tmpAttr->getValue() << endl;
  
  //tmpAttr = elem1->getAttributeNode("Attr1");
  //elem1->removeAttributeNode(tmpAttr);
  //cout << "elem1 attr1 (after removal)=" << elem1->getAttribute("Attr1") << endl;
 
  //elem1->removeAttribute("Attr2");
  //cout << "elem1 attr2 (after removal)=" << elem1->getAttribute("Attr2") << endl;

  //tmpAttr = elem1->getAttributeNode("Attr2");
  //cout << "tmpAttr name (after removal)=" << tmpAttr->getName() << endl;
  //cout << "tmpAttr value (after removal)=" << tmpAttr->getValue() << endl;

  cout << endl << "--- Check out the Node List Stuff ---" << endl;

  subelem1 = document->createElement("Sub Elem0");
  subelem2 = document->createElement("Sub Elem1");

  elem1->appendChild(subelem1);
  elem1->appendChild(subelem2);

  subElems = elem1->getChildNodes();

  cout << "subElems length=" << subElems->getLength() << endl;

  tmpElem = (Element*)subElems->item(0);

  cout << "tmpElem name=" << tmpElem->getTagName() << endl;

  cout << endl << "--- Check out the Named Node Map Stuff ---" << endl;

  attrs = elem1->getAttributes();

  cout << "attrs length=" << attrs->getLength() << endl;

  tmpAttr = (Attr*)attrs->item(1);
  
  cout << tmpAttr->getName() << "=" << tmpAttr->getValue() << endl; 

  delete nsxmlDocument;
  delete document;
}
