
#ifndef PROJ_PATH
BASE_PATH = ../../../base
DOM_PATH = .
#endif

INCLUDE_PATHS = -I$(BASE_PATH) -I. -I-


CC := $(CC) -g $(INCLUDE_PATHS)

DOM_OBJS = NodeDefinition.o           \
           Document.o                 \
	       DocumentFragment.o         \
	       NamedNodeMap.o             \
	       NodeListDefinition.o       \
	       Element.o                  \
           Attr.o                     \
           CharacterData.o            \
           Text.o                     \
           Comment.o                  \
           CDATASection.o             \
           ProcessingInstruction.o    \
           Notation.o                 \
           Entity.o                   \
           EntityReference.o          \
           DocumentType.o             \
           DOMImplementation.o

target: $(DOM_OBJS)

NodeDefinition.o:  NodeDefinition.cpp dom.h
	$(CC) -c NodeDefinition.cpp

Document.o: Document.cpp dom.h
	$(CC) -c Document.cpp

DocumentFragment.o: DocumentFragment.cpp dom.h
	$(CC) -c DocumentFragment.cpp

NamedNodeMap.o: NamedNodeMap.cpp dom.h
	$(CC) -c NamedNodeMap.cpp

NodeListDefinition.o: NodeListDefinition.cpp dom.h
	$(CC) -c NodeListDefinition.cpp

Element.o: Element.cpp dom.h
	$(CC) -c Element.cpp

Attr.o: Attr.cpp dom.h
	$(CC) -c Attr.cpp

CharacterData.o: CharacterData.cpp dom.h
	$(CC) -c CharacterData.cpp

Text.o: Text.cpp dom.h
	$(CC) -c Text.cpp

Comment.o: Comment.cpp dom.h
	$(CC) -c Comment.cpp

CDATASection.o: CDATASection.cpp dom.h
	$(CC) -c CDATASection.cpp

ProcessingInstruction.o: ProcessingInstruction.cpp dom.h
	$(CC) -c ProcessingInstruction.cpp

Notation.o: Notation.cpp dom.h
	$(CC) -c Notation.cpp

Entity.o: Entity.cpp dom.h
	$(CC) -c Entity.cpp

EntityReference.o: EntityReference.cpp dom.h
	$(CC) -c EntityReference.cpp

DocumentType.o: DocumentType.cpp dom.h
	$(CC) -c DocumentType.cpp

DOMImplementation.o: DOMImplementation.cpp dom.h
	$(CC) -c DOMImplementation.cpp





