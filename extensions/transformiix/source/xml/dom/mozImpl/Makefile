target: make_mozilla_dom

MOZILLA_INCLUDE = -I../../../../../../dist/include \
                  -I../../../../../../layout/xml/document/src \
                  -I../../../../../../layout/html/document/src \
                  -I../../../../../../layout/base/src
BASE_INCLUDE = ../../../base

CC := $(CC) -g -fno-rtti -fno-exceptions $(MOZILLA_INCLUDE) -I$(BASE_INCLUDE)


MOZILLA_OBJS = MozillaDOMImplementation.o \
               MozillaDocumentFragment.o \
               MozillaNotation.o \
               MozillaDocumentType.o \
               MozillaProcInstruction.o \
               MozillaEntity.o \
               MozillaEntityReference.o \
               MozillaCDATASection.o \
               MozillaComment.o \
               MozillaText.o \
               MozillaCharacterData.o \
               MozillaNamedNodeMap.o \
               MozillaNodeList.o \
               MozillaElement.o \
               MozillaDocument.o \
               MozillaNode.o \
               MozillaAttr.o \
               MozillaObjectWrapper.o

make_mozilla_dom: $(MOZILLA_OBJS)

MozillaDOMImplementation.o: MozillaDOMImplementation.cpp mozilladom.h
	$(CC) -c MozillaDOMImplementation.cpp

MozillaDocumentFragment.o: MozillaDocumentFragment.cpp mozilladom.h
	$(CC) -c MozillaDocumentFragment.cpp

MozillaNotation.o: MozillaNotation.cpp mozilladom.h
	$(CC) -c MozillaNotation.cpp

MozillaDocumentType.o: MozillaDocumentType.cpp mozilladom.h
	$(CC) -c MozillaDocumentType.cpp

MozillaProcInstruction.o: MozillaProcInstruction.cpp mozilladom.h
	$(CC) -c MozillaProcInstruction.cpp

MozillaEntity.o: MozillaEntity.cpp mozilladom.h
	$(CC) -c MozillaEntity.cpp

MozillaEntityReference.o: MozillaEntityReference.cpp mozilladom.h
	$(CC) -c MozillaEntityReference.cpp

MozillaCDATASection.o: MozillaCDATASection.cpp mozilladom.h
	$(CC) -c MozillaCDATASection.cpp

MozillaComment.o: MozillaComment.cpp mozilladom.h
	$(CC) -c MozillaComment.cpp

MozillaText.o: MozillaText.cpp mozilladom.h
	$(CC) -c MozillaText.cpp

MozillaCharacterData.o: MozillaCharacterData.cpp mozilladom.h
	$(CC) -c MozillaCharacterData.cpp

MozillaNamedNodeMap.o: MozillaNamedNodeMap.cpp mozilladom.h
	$(CC) -c MozillaNamedNodeMap.cpp

MozillaNodeList.o: MozillaNodeList.cpp mozilladom.h
	$(CC) -c MozillaNodeList.cpp

MozillaElement.o: MozillaElement.cpp mozilladom.h
	$(CC) -c MozillaElement.cpp

MozillaDocument.o: MozillaDocument.cpp mozilladom.h
	$(CC) -c MozillaDocument.cpp

MozillaNode.o: MozillaNode.cpp mozilladom.h
	$(CC) -c MozillaNode.cpp

MozillaAttr.o: MozillaAttr.cpp mozilladom.h
	$(CC) -c MozillaAttr.cpp

MozillaObjectWrapper.o: MozillaObjectWrapper.cpp mozilladom.h
	$(CC) -c MozillaObjectWrapper.cpp






