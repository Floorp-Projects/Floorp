#! This tmake template is for generating a makefile to build
#! custom stuff (icons and moc) for Windows.
#! Get tmake from ftp.troll.no/freebies/tmake

#${
    $project{"HEADERS"} = join(" ",find_files(".",'\.h$',0));
    $project{"SOURCES"} = join(" ",find_files(".",'\.cpp$',0));
    $moc_aware = 1;
    StdInit();
    @img = find_files("icons/images/","^(TB|BM).*\.gif",0);
    push @img, find_files("icons/images/","^Dash_.*ecure\.gif",0);
    push @img, find_files("images/",".*anim.*\.gif");
    $project{"ICONS"} = join(" ",@img);    
#$}

ICONS	=	#$ ExpandList("ICONS");

SRCMOC	=	#$ ExpandList("SRCMOC");

MOC	=	moc

all: icon-pixmaps.inc $(SRCMOC)

icon-pixmaps.inc: $(ICONS) $(SRCMOC)
	mkfiles32\embed $(ICONS) > icon-pixmaps.inc

#$ BuildMocSrc(Project("HEADERS"));
#$ BuildMocSrc(Project("SOURCES"));
