The contents of this directory constitute the UDDI support of mozilla.
Currently the UDDI Inquiry API is completely implemented, but not tested. In
addition it is neccessary to add these files to a chrome resource somewhere in
order to make the calls. During development I had these file in the
mozilla/extensions/webservices/build/src directory and included the js files
in a jar.mn in the same directory.

The implementation is merely calls to the native SOAP layer, but there are
enough types involved that we felt it would be helpful to have an abstraction.
So we came up with a similar concept to what we did with SOAP, namely to have
a UDDICall object that served as a proxy and did the SOAP calls underneath
the covers. 

The final goal of this implementation was to have the UDDI support actually
be a js component. This would eliminate the need to have the js files and the
test html page be loaded in a chrome url. Didn't get far enough to get that
work started.

Things left to do:
 - add encoders for the return types. currently the return types just have
   decoders. The encoders are desired for outputting the return types as
   text strings.
 - create a js component
 - test the implementation to make sure all the fields in the js objects are
   actually getting populated as they should. This is mainly focused at the
   decoding of the objects (which is why we need the first task handled.
 - take the js code in the uddi.html file and create a seperate library that
   includes those methods for testing purposes.
 - transform the uddi.html file into a full coverage UDDI testing page that
   will not only allow for the testing of methods, but will populate fields
   with the return values. (think test automation too!)

john gaunt
redfive@acm.org
7/21/2003