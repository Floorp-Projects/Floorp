#
# This requires -vpackage_name=name.of.the.package -vvalue=[true|false]
#

BEGIN{
print
print "// generated automatically by gen_dbg.awk"
print
print "package "package_name";" 
print
print "public interface AS"
print "{"
print "    public static final boolean S     = "value";"
print "    public static final boolean DEBUG = "value";"
print "}"
}
