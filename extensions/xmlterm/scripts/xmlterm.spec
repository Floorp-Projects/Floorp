Name:      xmlterm
Version:   M14
Release:   1
Summary:   A graphical command line interface using Mozilla
Copyright: Mozilla Public License 1.1
Group:     User Interface/X
Source:    http://xmlterm.org/xmlterm-source-Mar7-M14.tar.gz
URL:       http://xmlterm.org/
Vendor:    xmlterm.org
Packager:  R. Saravanan <svn@xmlterm.org>
Prefix:    /usr/src/redhat/BUILD/package

AutoReqProv: no

%description

 XMLterm: A graphical command line interface using Mozilla

%prep

%build

%install
rm -fr xmlterm.tgz package
tar czvhf xmlterm.tgz \
          -C /home/svn/mozilla/dist/bin \
             INSTALL.xmlterm xmlterm xcat xls menuhack \
             components/libxmlterm.so \
             components/xmlterm.xpt \
             chrome/xmlterm
mkdir package
cd package
tar zxvf ../xmlterm.tgz

chown -R root.root .
chmod -R a+rX,g-w,o-w .

%clean
rm -fr xmlterm.tgz package

%pre
cd $RPM_INSTALL_PREFIX
if [ ! -f run-mozilla.sh ]
then
   echo "*ERROR* Specify the mozilla package directory using the RPM prefix option"
   echo "   rpm --prefix /path/package ..."
   exit 1
fi

%post
for FILE in xmlterm xcat xls
do
   ln -s $RPM_INSTALL_PREFIX/$FILE /usr/bin/$FILE
done

cd $RPM_INSTALL_PREFIX
rm -f component.reg
##./menuhack

%postun
for FILE in xmlterm xcat xls
do
   rm /usr/bin/$FILE
done

%files

/usr/src/redhat/BUILD/package/INSTALL.xmlterm
/usr/src/redhat/BUILD/package/xmlterm
/usr/src/redhat/BUILD/package/xcat
/usr/src/redhat/BUILD/package/xls
/usr/src/redhat/BUILD/package/menuhack
/usr/src/redhat/BUILD/package/components/libxmlterm.so
/usr/src/redhat/BUILD/package/components/xmlterm.xpt
/usr/src/redhat/BUILD/package/chrome/xmlterm

%changelog
