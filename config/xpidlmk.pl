#
# call with
# argv[0] = Operating system
# argv[1] = depth
# argv[2] = current dir
# argv[3] = generated file dir
# argv[4] = target export .H and .idl base dir
# argv[5] = target .xpt dir
# argv[6] = module name
#
# argv[...] = the idl files
# 

# get and validate the args

$#ARGV >= 0 or die "not enough args";
$OS_ARCH = shift;

$#ARGV >= 0 or die "not enough args";
$depth = shift;

$#ARGV >= 0 or die "not enough args";
$idl_src_dir = shift;

$#ARGV >= 0 or die "not enough args";
$gen_dir = shift;

$#ARGV >= 0 or die "not enough args";
$dist_dir = shift;

$#ARGV >= 0 or die "not enough args";
$dist_typelib_dir = shift;

$#ARGV >= 0 or die "not enough args";
$module = shift;

#these need to switch on platform (Win32 v. Unix)

if($OS_ARCH eq "WINNT" || $OS_ARCH eq "OS2") {
    $isUnix = 0;
    $SEP = "\\";
    $RULES_FILE = "rules.mak";
    $REDIR_QUIET = " >NUL";
    $QUIET = "@";
    $INC_L_ANGLE="<";
    $INC_R_ANGLE=">";
} else {
    $isUnix = 1;
    $SEP = "/";
    $RULES_FILE = "rules.mk";
    $REDIR_QUIET = " >/dev/null";
    $QUIET = "@";
}

# these should be the same on all platforms

$SHOW = "\@echo";
$XPIDL_PROG = "\$(XPIDL_PROG)";
$XPTLINK = "\$(XPTLINK_PROG)";
$MAKE_DIR = "mkdir";
$RM_R = "\$(RM_R)";
$RM = "\$(RM)";
$NSINSTALL = "\$(NSINSTALL)";
$CONFIG = "config";

# construct these strings used for each file

if($isUnix) {
    $dist_header_dir = "$dist_dir$SEP"."include";
} else {
    $dist_header_dir = "$dist_dir$SEP"."public"."$SEP$module";
}
$dist_idl_dir    = "$dist_dir$SEP"."idl";
$gen_linked_typelib_file = "$gen_dir$SEP$module.xpt";
$dist_linked_typelib_file = "$dist_typelib_dir$SEP$module.xpt";

#each typelib's name is added to this list
$gen_typelib_list = "";

#########################################################
# start emitting stuff

print("\n");
print("DEPTH=$depth\n");
print("\n");
print("include $INC_L_ANGLE\$(DEPTH)$SEP$CONFIG$SEP$RULES_FILE$INC_R_ANGLE\n");
print("\n");


# for each .idl file in the list...

while ($#ARGV >= 0) {
    $basename = $idlfile = shift;
    $basename =~ s/(.+)\..+/\1/;

#    print("$idlfile, $basename\n");

    $idl_src_file      = "$idl_src_dir$SEP$basename.idl";
    $dist_idl_file     = "$dist_idl_dir$SEP$basename.idl";
    $dist_header_file  = "$dist_header_dir$SEP$basename.h";
    $gen_filename_base = "$gen_dir$SEP$basename";
    $gen_header_file   = "$gen_filename_base.h";
    $gen_typelib_file  = "$gen_filename_base.xpt";


    #########################################################
    # copy idl file to dist (XXX assume that dist exists)
    
    print("$dist_idl_file : $idl_src_file\n");
#    print("\t$SHOW copying $idl_src_file to dist \n");
    print("\t$QUIET$NSINSTALL $idl_src_file $dist_idl_dir $REDIR_QUIET\n");
    print("\n");

    #########################################################
    # generate header and copy to dist (XXX assume that dist exists)
    print("$dist_header_file : $idl_src_file\n");
    print("\t$SHOW XPIDL is generating $basename.h from $basename.idl\n");
    print("\t$QUIET$XPIDL_PROG -w -m header -I $dist_idl_dir -o $gen_filename_base $idl_src_file\n");
#    print("\t$SHOW copying $gen_header_file to dist \n");
    print("\t$QUIET$NSINSTALL $gen_header_file $dist_header_dir $REDIR_QUIET\n");
    print("\n");

    #########################################################
    # generated individual typelib

    print("$gen_typelib_file : $idl_src_file\n");
    print("\t$SHOW XPIDL is generating $basename.xpt from $basename.idl\n");
    print("\t$QUIET$XPIDL_PROG -w -m typelib -I $dist_idl_dir -o $gen_filename_base $idl_src_file\n");
    print("\n");

    $gen_typelib_list .= "$gen_typelib_file ";

    #########################################################
    ## export relies on our targets...

    print("doxpidl :: $dist_idl_file $dist_header_file\n");
    print("\n");

    #########################################################
    ## all the clean up

    print("clobber ::\n");
    print("\t$QUIET$RM $dist_idl_file $REDIR_QUIET\n");
    print("\t$QUIET$RM $dist_header_file $REDIR_QUIET\n");
    print("\t$QUIET$RM $gen_header_file $REDIR_QUIET\n");
    print("\t$QUIET$RM $gen_typelib_file $REDIR_QUIET\n");
    print("\n");
}

#########################################################
## the typelib link 

print("$dist_linked_typelib_file : $gen_typelib_list\n");
print("\t$SHOW xpt_link is linking $gen_typelib_list\n");
print("\t$QUIET$XPTLINK $gen_linked_typelib_file $gen_typelib_list\n");
print("\t$QUIET$NSINSTALL $gen_linked_typelib_file $dist_typelib_dir $REDIR_QUIET\n");
print("\n");

print("doxpidl :: $dist_linked_typelib_file\n");
print("\n");

print("clobber ::\n");
print("\t$QUIET$RM $dist_linked_typelib_file $REDIR_QUIET\n");
print("\t$QUIET$RM $gen_linked_typelib_file $REDIR_QUIET\n");
print("\n");

