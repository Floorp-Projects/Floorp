HOMEDIR=../
SRCDIR=$(HOMEDIR)/src
CLASSDIR=$(HOMEDIR)/classes

JAVAC=javac
TESTPACKAGE=org/mozilla/dom/test
DOMPACKAGE=org/mozilla/dom

all: 
	@echo "******************************************************"
	@echo "Make changes to $(SRCDIR)/mozilla.csh"
	@echo "and source it else it may not compile..."
	@echo "******************************************************"
	@echo ""
	$(JAVAC) -d $(CLASSDIR) $(JAVAFILES) 
	$(JAVAC) -d $(CLASSDIR) $(DOMFILE) 
	cd  api; make

JAVAFILES= Execution.java \
	   BWBaseTest.java \
	   TestLoader.java \
	   ParamCombination.java \
	   ReflectionTest.java \
	   BWJavaTemplate.java

DOMFILE=  DOMFactory.java \
	  DOMAccessorImpl.java

clean:
	for i in ${JAVAFILES} ; do \
		file=$$i; \
		classfile=`echo $$file | sed 's/java/class/g'` ; \
		/bin/rm $(CLASSDIR)/$(TESTPACKAGE)/$$classfile ; done
	/bin/rm $(CLASSDIR)/$(DOMPACKAGE)/DOMFactory.class ;
	cd api; make clean
	
	
