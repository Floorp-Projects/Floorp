setenv BLACKWOOD_HOME `pwd`/..
setenv MOZILLA_FIVE_HOME `pwd`/../../../../dist/bin
setenv JAVA_HOME /usr/local/java/jdk1.2/solaris 
setenv TEST_PATH  ${BLACKWOOD_HOME}
setenv XML_PATH /workspace/xml
setenv PREFIX /workspace


setenv LD_LIBRARY_PATH ${JAVA_HOME}/jre/lib/sparc
setenv LD_LIBRARY_PATH ${LD_LIBRARY_PATH}:${JAVA_HOME}/jre/lib/sparc/classic
setenv LD_LIBRARY_PATH ${LD_LIBRARY_PATH}:${JAVA_HOME}/jre/lib/sparc/native_threads
setenv LD_LIBRARY_PATH ${LD_LIBRARY_PATH}:/workspace/lib:/workspace/mozilla/dist/lib
setenv LD_LIBRARY_PATH ${LD_LIBRARY_PATH}:${MOZILLA_FIVE_HOME}

setenv PATH /bin:/usr/bin:/usr/ccs/bin:/usr/dt/bin
setenv PATH ${PATH}:/usr/dist/local/exe:/usr/dist/exe
setenv PATH ${JAVA_HOME}/bin:${PATH}
setenv PATH ${PATH}:${MOZILLA_FIVE_HOME}:/workspace/bin

setenv CLASSPATH ${MOZILLA_FIVE_HOME}/classes:${XML_PATH}/xml.jar:.
setenv CLASSPATH ${TEST_PATH}/classes:${CLASSPATH}
