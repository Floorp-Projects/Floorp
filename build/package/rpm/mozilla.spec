Summary:		Mozilla and stuff
Name:			mozilla
Version:		5.0
Release:		0
Serial:			0
Copyright:		NPL/MPL
Group:			Mozilla
Source0:		mozilla-dist.tar.gz
Buildroot:		/var/tmp/mozilla-root
Prefix:			/usr
Requires:		gtk+ >= 1.2.4

#Exclusivearch:		i386

%description
Mozilla is the king of all beasts - big badass Beasts.

%package nspr
Summary:	nspr
Group:		Mozilla

%description nspr
nspr

%package nspr-devel
Requires:	nspr
Summary:	nspr-devel
Group:		Mozilla

%description nspr-devel
nspr devel

%package core
Summary:	core
Group:		Mozilla
Requires:	nspr

%description core
core

%package core-devel
Requires:	core
Summary:	core-devel
Group:		Mozilla
Requires:	nspr-devel

%description core-devel
core devel

%package network
Summary:	network
Group:		Mozilla
Requires:	core

%description network
network

%package network-devel
Requires:	network
Summary:	network-devel
Group:		Mozilla
Requires:	core-devel

%description network-devel
network devel

%package layout
Summary:	layout
Group:		Mozilla
Requires:	network

%description layout
layout

%package layout-devel
Requires:	layout
Summary:	layout-devel
Group:		Mozilla
Requires:	network-devel

%description layout-devel
layout devel

%package xpinstall
Summary:	xpinstall
Group:		Mozilla
Requires:	layout

%description xpinstall
xpinstall

%package xpinstall-devel
Requires:	xpinstall
Summary:	xpinstall-devel
Group:		Mozilla
Requires:	layout-devel

%description xpinstall-devel
xpinstall devel

%package profile
Summary:	profile
Group:		Mozilla
Requires:	layout

%description profile
profile

%package profile-devel
Requires:	profile
Summary:	profile-devel
Group:		Mozilla
Requires:	layout-devel

%description profile-devel
profile devel

%package xptoolkit
Summary:	xptoolkit
Group:		Mozilla
Requires:	layout

%description xptoolkit
xptoolkit

%package xptoolkit-devel
Requires:	xptoolkit
Summary:	xptoolkit-devel
Group:		Mozilla
Requires:	layout-devel

%description xptoolkit-devel
xptoolkit devel

%package cookie
Summary:	cookie
Group:		Mozilla
Requires:	layout

%description cookie
cookie

%package cookie-devel
Requires:	cookie
Summary:	cookie-devel
Group:		Mozilla
Requires:	layout-devel

%description cookie-devel
cookie devel

%package wallet
Summary:	wallet
Group:		Mozilla
Requires:	layout

%description wallet
wallet

%package wallet-devel
Requires:	wallet
Summary:	wallet-devel
Group:		Mozilla
Requires:	layout-devel

%description wallet-devel
wallet devel

%prep
%setup -n mozilla

%install
rm -rf $RPM_BUILD_ROOT

################################
#
# Remember where we are
#
################################
here=`pwd`

################################
#
# configure
#
################################

###
###
###
rm -f $here/blank
touch $here/blank
MOZCONFIG=blank
export MOZCONFIG

./configure --disable-debug --enable-optimize --disable-mailnews --disable-tests --with-xlib=no --with-motif=no --enable-strip-libs

make

################################

mkdir -p $RPM_BUILD_ROOT/%{prefix}/lib/mozilla

################################
#
# Generate the package lists
#
################################
here=`pwd`
mkdir -p $here/file-lists

cd build/package/rpm

./generate-package-info.sh mozilla-package-list.txt . $here/file-lists $here

cd $here
################################

################################
#
# Copy the stuff in dist/* to the rpm stage place
#
################################
cp -r dist/* $RPM_BUILD_ROOT/%{prefix}/lib/mozilla/

cd $RPM_BUILD_ROOT/%{prefix}/lib/mozilla/

/bin/mv -f bin/*.so lib
/bin/mv -f bin/chrome .
/bin/mv -f bin/components .
/bin/mv -f bin/defaults .
/bin/mv -f bin/netscape.cfg .
/bin/mv -f bin/res .

cd $here
################################

%clean
rm -rf $RPM_BUILD_ROOT

%files -f file-lists/mozilla-nspr-file-list.txt nspr
%defattr(-,root,root)

%files -f file-lists/mozilla-nspr-devel-file-list.txt nspr-devel
%defattr(-,root,root)

%files -f file-lists/mozilla-core-file-list.txt core
%defattr(-,root,root)

%files -f file-lists/mozilla-core-devel-file-list.txt core-devel
%defattr(-,root,root)

%files -f file-lists/mozilla-network-file-list.txt network
%defattr(-,root,root)

%files -f file-lists/mozilla-network-devel-file-list.txt network-devel
%defattr(-,root,root)

%files -f file-lists/mozilla-layout-file-list.txt layout
%defattr(-,root,root)

%files -f file-lists/mozilla-layout-devel-file-list.txt layout-devel
%defattr(-,root,root)

%files -f file-lists/mozilla-xpinstall-file-list.txt xpinstall
%defattr(-,root,root)

%files -f file-lists/mozilla-xpinstall-devel-file-list.txt xpinstall-devel
%defattr(-,root,root)

%files -f file-lists/mozilla-profile-file-list.txt profile
%defattr(-,root,root)

%files -f file-lists/mozilla-profile-devel-file-list.txt profile-devel
%defattr(-,root,root)

%files -f file-lists/mozilla-xptoolkit-file-list.txt xptoolkit
%defattr(-,root,root)

%files -f file-lists/mozilla-xptoolkit-devel-file-list.txt xptoolkit-devel
%defattr(-,root,root)

%files -f file-lists/mozilla-cookie-file-list.txt cookie
%defattr(-,root,root)

%files -f file-lists/mozilla-cookie-devel-file-list.txt cookie-devel
%defattr(-,root,root)

%files -f file-lists/mozilla-wallet-file-list.txt wallet
%defattr(-,root,root)

%files -f file-lists/mozilla-wallet-devel-file-list.txt wallet-devel
%defattr(-,root,root)

%changelog
* Wed Oct 20 1999 Ramiro Estrugo <ramiro@fateware.com>
- First rev.

