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

#ifndef qgeckoplugin_h
#define qgeckoplugin_h

#include <qwidgetplugin.h>

/**
 * Implements an interface for
 * the Qt-Designer plugin
 */
class QGeckoPlugin : public QWidgetPlugin
{
public:

    QGeckoPlugin();

    QStringList keys() const;
    QWidget* create(const QString& key,
                    QWidget* parent = 0,
                    const char* name = 0);
    QString group(const QString& key) const;
    QIconSet iconSet(const QString& key) const;
    QString includeFile(const QString& key) const;
    QString toolTip(const QString& key) const;
    QString whatsThis(const QString& key) const;
    bool isContainer(const QString&) const;

};

#endif /* qgeckoplugin_h */
