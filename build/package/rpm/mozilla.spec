Summary:		Mozilla and stuff
Name:			mozilla
Version:		5.0
Release:		0
Serial:			0
Copyright:		NPL/MPL
Group:			Mozilla
Source0:		ftp://ftp.mozilla.org/pub/mozilla/nightly/latest/mozilla-source.tar.gz
#Source0:		ftp://ftp.mozilla.org/pub/mozilla/nightly/latest/mozilla-binary.tar.gz
Source1:		mozilla-run-regxpcom.sh
Buildroot:		/var/tmp/mozilla-root
Prefix:			/usr
Requires:		gtk+ >= 1.2.4

#Exclusivearch:		i386

%description
Mozilla is the king of all beasts - big badass Beasts.

%package nspr
Summary:	mozilla-nspr
Group:		Mozilla

%description nspr
mozilla-nspr

%package nspr-devel
Requires:	mozilla-nspr
Summary:	mozilla-nspr-devel
Group:		Mozilla

%description nspr-devel
mozilla-nspr devel

%package core
Summary:	mozilla-core
Group:		Mozilla
Requires:	mozilla-nspr

%description core
mozilla-core

%package core-devel
Requires:	mozilla-core
Summary:	mozilla-core-devel
Group:		Mozilla
Requires:	mozilla-nspr-devel

%description core-devel
mozilla-core devel

%package network
Summary:	mozilla-network
Group:		Mozilla
Requires:	mozilla-core

%description network
mozilla-network

%package network-devel
Requires:	mozilla-network
Summary:	mozilla-network-devel
Group:		Mozilla
Requires:	mozilla-core-devel

%description network-devel
mozilla-network devel

%package layout
Summary:	mozilla-layout
Group:		Mozilla
Requires:	mozilla-network

%description layout
mozilla-layout

%package layout-devel
Requires:	mozilla-layout
Summary:	mozilla-layout-devel
Group:		Mozilla
Requires:	mozilla-network-devel

%description layout-devel
mozilla-layout devel

%package xpinstall
Summary:	mozilla-xpinstall
Group:		Mozilla
Requires:	mozilla-layout

%description xpinstall
mozilla-xpinstall

%package xpinstall-devel
Requires:	mozilla-xpinstall
Summary:	mozilla-xpinstall-devel
Group:		Mozilla
Requires:	mozilla-layout-devel

%description xpinstall-devel
mozilla-xpinstall devel

%package profile
Summary:	mozilla-profile
Group:		Mozilla
Requires:	mozilla-layout

%description profile
mozilla-profile

%package profile-devel
Requires:	mozilla-profile
Summary:	mozilla-profile-devel
Group:		Mozilla
Requires:	mozilla-layout-devel

%description profile-devel
mozilla-profile devel

%package xptoolkit
Summary:	mozilla-xptoolkit
Group:		Mozilla
Requires:	mozilla-layout

%description xptoolkit
mozilla-xptoolkit

%package xptoolkit-devel
Requires:	mozilla-xptoolkit
Summary:	mozilla-xptoolkit-devel
Group:		Mozilla
Requires:	mozilla-layout-devel

%description xptoolkit-devel
mozilla-xptoolkit devel

%package cookie
Summary:	mozilla-cookie
Group:		Mozilla
Requires:	mozilla-layout

%description cookie
mozilla-cookie

%package cookie-devel
Requires:	mozilla-cookie
Summary:	mozilla-cookie-devel
Group:		Mozilla
Requires:	mozilla-layout-devel

%description cookie-devel
mozilla-cookie devel

%package wallet
Summary:	mozilla-wallet
Group:		Mozilla
Requires:	mozilla-layout

%description wallet
mozilla-wallet

%package wallet-devel
Requires:	mozilla-wallet
Summary:	mozilla-wallet-devel
Group:		Mozilla
Requires:	mozilla-layout-devel

%description wallet-devel
mozilla-wallet devel

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

if [ 1 ]
then
###
###
###
rm -f $here/blank
touch $here/blank
MOZCONFIG=blank
export MOZCONFIG

./configure --disable-mailnews --disable-tests --with-xlib=no --with-motif=no --disable-gtk-mozilla

#./configure --disable-debug --enable-optimize --disable-mailnews --disable-tests --with-xlib=no --with-motif=no --enable-strip-libs --disable-gtk-mozilla

make
fi
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

install -m 755 build/package/rpm/mozilla $RPM_BUILD_ROOT/%{prefix}/lib/mozilla/bin

install -m 755 $RPM_SOURCE_DIR/mozilla-run-regxpcom.sh $RPM_BUILD_ROOT/%{prefix}/lib/mozilla/bin

touch $RPM_BUILD_ROOT/%{prefix}/lib/mozilla/component.reg
################################

%clean
rm -rf $RPM_BUILD_ROOT

%files -f file-lists/mozilla-nspr-file-list.txt nspr
%defattr(-,root,root)

%files -f file-lists/mozilla-nspr-devel-file-list.txt nspr-devel
%defattr(-,root,root)

%files -f file-lists/mozilla-core-file-list.txt core
%defattr(-,root,root)
%config(missingok) %{prefix}/lib/mozilla/component.reg
%{prefix}/lib/mozilla/bin/mozilla-run-regxpcom.sh %{prefix}/lib/mozilla

%files -f file-lists/mozilla-core-devel-file-list.txt core-devel
%defattr(-,root,root)

%post core
%{prefix}/lib/mozilla/bin/mozilla-run-regxpcom.sh %{prefix}/lib/mozilla

%files -f file-lists/mozilla-network-file-list.txt network
%defattr(-,root,root)

%files -f file-lists/mozilla-network-devel-file-list.txt network-devel
%defattr(-,root,root)

%post network
%{prefix}/lib/mozilla/bin/mozilla-run-regxpcom.sh %{prefix}/lib/mozilla

%files -f file-lists/mozilla-layout-file-list.txt layout
%defattr(-,root,root)

%files -f file-lists/mozilla-layout-devel-file-list.txt layout-devel
%defattr(-,root,root)

%post layout
%{prefix}/lib/mozilla/bin/mozilla-run-regxpcom.sh %{prefix}/lib/mozilla

%files -f file-lists/mozilla-xpinstall-file-list.txt xpinstall
%defattr(-,root,root)

%files -f file-lists/mozilla-xpinstall-devel-file-list.txt xpinstall-devel
%defattr(-,root,root)

%post xpinstall
%{prefix}/lib/mozilla/bin/mozilla-run-regxpcom.sh %{prefix}/lib/mozilla

%files -f file-lists/mozilla-profile-file-list.txt profile
%defattr(-,root,root)

%files -f file-lists/mozilla-profile-devel-file-list.txt profile-devel
%defattr(-,root,root)

%post profile
%{prefix}/lib/mozilla/bin/mozilla-run-regxpcom.sh %{prefix}/lib/mozilla

%files -f file-lists/mozilla-xptoolkit-file-list.txt xptoolkit
%defattr(-,root,root)
%{prefix}/lib/mozilla/bin/mozilla

%files -f file-lists/mozilla-xptoolkit-devel-file-list.txt xptoolkit-devel
%defattr(-,root,root)

%post xptoolkit
%{prefix}/lib/mozilla/bin/mozilla-run-regxpcom.sh %{prefix}/lib/mozilla

%files -f file-lists/mozilla-cookie-file-list.txt cookie
%defattr(-,root,root)

%files -f file-lists/mozilla-cookie-devel-file-list.txt cookie-devel
%defattr(-,root,root)

%post cookie
%{prefix}/lib/mozilla/bin/mozilla-run-regxpcom.sh %{prefix}/lib/mozilla

%files -f file-lists/mozilla-wallet-file-list.txt wallet
%defattr(-,root,root)

%files -f file-lists/mozilla-wallet-devel-file-list.txt wallet-devel
%defattr(-,root,root)

%post wallet
%{prefix}/lib/mozilla/bin/mozilla-run-regxpcom.sh %{prefix}/lib/mozilla

%changelog
* Wed Oct 20 1999 Ramiro Estrugo <ramiro@fateware.com>
- First rev.

