Mach-O Build Notes

1. The Mach-O build of PPEmbed is made with PowerPlant from CW Pro 8. Project
Builder has no idea where PowerPlant is installed on your drive. PPEmbed.pbproj
is set up with PowerPlant located, by absolute path, at /Developer/PowerPlant.
Needless to say, that's not where it exists on your drive, so a symlink is needed.
Open a terminal window and do this:

% cd /Developer
% ln -s <Drag the PowerPlant folder into the terminal window>

You can type the whole path to the PowerPlant folder if you like, but it's a
long, gruesome path with spaces.

2. A few changes need to be made to PowerPlant in order to get it to compile with
gcc. Fortunately, these don't prevent it from being compiled with CW. The diffs
are included below. They cannot easily be applied with patch, but do illustrate
the changes.

================================================================================
diff -u -t orig-PP-files/LException.cp changed-PP-files/LException.cp
--- orig-PP-files/LException.cp	Thu Jan 23 14:41:59 2003
+++ changed-PP-files/LException.cp	Thu Jan 23 14:41:20 2003
@@ -91,7 +91,7 @@
 //      ¥ ~LException                                                    Destructor                                [public]
 // ---------------------------------------------------------------------------
 
-LException::~LException()
+LException::~LException() throw()
 {
 }
 
@@ -102,7 +102,7 @@
 //      Returns the error message as a C string. Inherited from std::exception.
 
 const char*
-LException::what() const
+LException::what() const throw()
 {
         StringPtr       lastPtr = (StringPtr) &mErrorString[mErrorString[0]];
 
================================================================================ 
diff -u -t orig-PP-files/LException.h changed-PP-files/LException.h
--- orig-PP-files/LException.h	Thu Jan 23 14:41:49 2003
+++ changed-PP-files/LException.h	Thu Jan 23 14:41:01 2003
@@ -30,9 +30,9 @@
 
         LException&                     operator = ( const LException& inException );
 
-        virtual                         ~LException();
+        virtual                         ~LException() throw();
 
-        virtual const char*     what() const;
+        virtual const char*     what() const throw();
 
         SInt32                          GetErrorCode() const;
 
================================================================================ 
diff -u -t orig-PP-files/LGATabsControlImp.cp changed-PP-files/LGATabsControlImp.cp
--- orig-PP-files/LGATabsControlImp.cp	Thu Jan 23 14:41:54 2003
+++ changed-PP-files/LGATabsControlImp.cp	Thu Jan 23 14:41:08 2003
@@ -991,11 +991,11 @@
                                 tabButton->SetIconResourceID(info->iconSuiteID);
                         }
                         break;
+                }
                         
                 default:
                         LGAControlImp::SetDataTag(inPartCode, inTag, inDataSize, inDataPtr);
                         break;
-                }
         }
 }
 
================================================================================ 
diff -u -t orig-PP-files/LStream.h changed-PP-files/LStream.h
--- orig-PP-files/LStream.h	Thu Jan 23 14:41:43 2003
+++ changed-PP-files/LStream.h	Thu Jan 23 14:41:14 2003
@@ -154,13 +154,13 @@
                                                         (*this) << (double) inNum;
                                                         return (*this);
                                                 }
-                                                
+#ifdef __MWERKS__                                               
         LStream&                operator << (short double inNum)
                                                 {
                                                         (*this) << (double) inNum;
                                                         return (*this);
                                                 }
-
+#endif
         LStream&                operator << (bool inBool)
                                                 {
                                                         WriteBlock(&inBool, sizeof(inBool));
@@ -276,7 +276,7 @@
                                                         outNum = num;
                                                         return (*this);
                                                 }
-                                                
+#ifdef __MWERKS                                         
         LStream&                operator >> (short double &outNum)
                                                 {
                                                         double num;
@@ -284,7 +284,7 @@
                                                         outNum = (short double) num;
                                                         return (*this);
                                                 }
-
+#endif
         LStream&                operator >> (bool &outBool)
                                                 {
                                                         ReadBlock(&outBool, sizeof(outBool));
