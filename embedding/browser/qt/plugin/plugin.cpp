/* Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Michal Ceresna
 * for Lixto GmbH.
 *
 * Portions created by Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michal Ceresna <ceresna@amos.sturak.sk>
 */

#include <dlfcn.h>

#include <qdir.h>
#include <qlabel.h>

#include "qgeckoembed.h"
#include "plugin.h"
#include "plugin.xpm"

QGeckoPlugin::QGeckoPlugin()
{
}

QStringList QGeckoPlugin::keys() const
{
    QStringList list;
    list << "QGeckoEmbed";
    return list;
}

QWidget* QGeckoPlugin::create(const QString& key,
                              QWidget* parent,
                              const char* name)
{
    if (key=="QGeckoEmbed") {
        //nicer would be to use the QLibrary, but it uses only
        //the RTLD_LAZY flag and we need the RTLD_GLOBAL
        //to successfully load the rest of mozilla libs
        void *handle1 = dlopen("libxpcom.so", RTLD_NOW|RTLD_GLOBAL);
        void *handle2 = dlopen("libqgeckoembed.so", RTLD_NOW|RTLD_GLOBAL);
        if (!handle1 || !handle2) {
            QLabel *l = new QLabel(parent, name);
            l->setText("<html><body>"
                       "Unable to initialize Mozilla.<br>"
                       "Try to set <b>MOZILLA_FIVE_HOME</b> and "
#ifdef Q_WS_WIN
                       "<b>PATH</b>"
#else
                       "<b>LD_LIBRARY_PATH</b>"
#endif
                       " variables before starting the Qt Designer."
                       "</body></html>");
            return l;
        }
        else {
            //this is a workaround for mozilla bug:
            //call to NS_InitEmbedding works only once
            static bool initialized = FALSE;
            if (!initialized) {
                QGeckoEmbed::
                initialize(QDir::
                           convertSeparators(QDir::home().absPath()+
                                             "/.qgeckoembed-qtdesigner"),
                           "QtDesigner");
                //QGeckoGlobals::pushStartup()
                static QGeckoEmbed *sentinel;
                sentinel = new QGeckoEmbed(NULL, "sentinel");
                initialized = TRUE;
            }
            QGeckoEmbed *me = new QGeckoEmbed(parent, name);
            me->loadURL("about:");
            return me;
        }
    }
    else {
        return NULL;
    }

}

QString QGeckoPlugin::includeFile(const QString& key) const
{
    return
        key=="QGeckoEmbed"?
        "qgeckoembed.h":
        QString::null;
}

QString QGeckoPlugin::group(const QString& key) const
{
    return
        key=="QGeckoEmbed"?
        "Display (Mozilla)":
        QString::null;
}

QIconSet QGeckoPlugin::iconSet(const QString&) const
{
    return QIconSet(QPixmap(mozilla_pixmap));
}

QString QGeckoPlugin::toolTip(const QString& key) const
{
    return
        key=="QGeckoEmbed"?
        "Mozilla Browser Control":
        QString::null;
}

QString QGeckoPlugin::whatsThis(const QString& key) const
{
    return
        key=="QGeckoEmbed"?
        "A widget with Mozilla Web Browser":
        QString::null;
}


bool QGeckoPlugin::isContainer(const QString&) const
{
    return FALSE;
}

Q_EXPORT_PLUGIN(QGeckoPlugin)

