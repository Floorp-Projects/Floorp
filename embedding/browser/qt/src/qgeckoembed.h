/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *  Zack Rusin <zack@kde.org>.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Lars Knoll <knoll@kde.org>
 *   Zack Rusin <zack@kde.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef QECKOEMBED_H
#define QECKOEMBED_H

#include <qwidget.h>

class nsIDOMKeyEvent;
class nsIDOMMouseEvent;
class nsIDOMUIEvent;
class nsModuleComponentInfo;
class nsIDirectoryServiceProvider;
class nsIAppShell;
class nsVoidArray;
class nsProfileDirServiceProvider;
class nsIPref;
class nsISupports;
class EmbedWindow;
class EmbedEventListener;
class EmbedProgress;
class nsIWebNavigation;
class nsISHistory;
class nsIDOMEventReceiver;
class EmbedContentListener;
class EmbedStream;
class QHBox;
class nsIDOMDocument;
class nsPIDOMWindow;
class QPaintEvent;

class QGeckoEmbedPrivate;

class QGeckoEmbed : public QWidget
{
    Q_OBJECT
public:
    static void initialize(const char *aDir, const char *aName);
public:
    enum ReloadFlags
    {
        Normal,
        BypassCache,
        BypassProxy,
        BypassProxyAndCache,
        CharsetChange
    };
public:
    QGeckoEmbed(QWidget *parent, const char *name);
    ~QGeckoEmbed();

    bool canGoBack() const;
    bool canGoForward() const;

    void setIsChrome(bool);
    int chromeMask() const;

    nsIDOMDocument *document() const;
    QString url() const;
    QString resolvedUrl(const QString &relativepath) const;

public slots:
    void loadURL(const QString &url);
    void stopLoad();
    void goForward();
    void goBack();

    void renderData(const QCString &data, const QString &baseURI,
                    const QString &mimeType);

    int  openStream(const QString &baseURI, const QString &mimeType);
    int  appendData(const QCString &data);
    int  closeStream();

    void reload(ReloadFlags flags = Normal);

    void setChromeMask(int);

signals:
    void linkMessage(const QString &message);
    void jsStatusMessage(const QString &message);
    void locationChanged(const QString &location);
    void windowTitleChanged(const QString &title);

    void progress(int current, int max);
    void progressAll(const QString &url, int current, int max);

    void netState(int state, int status);
    void netStateAll(const QString &url, int state, int status);

    void netStart();
    void netStop();

    void newWindow(QGeckoEmbed *newWindow, int chromeMask);
    void visibilityChanged(bool visible);
    void destroyBrowser();
    void openURI(const QString &url);
    void sizeTo(int width, int height);

    void securityChange(void *request, int status, void *message);
    void statusChange(void *request, int status, void *message);

    void showContextMenu(const QPoint &p, const QString &url);

    /**
     * The dom signals are called only if the dom* methods
     * are not reimplemented.
     */
    void domKeyDown(nsIDOMKeyEvent *keyEvent);
    void domKeyPress(nsIDOMKeyEvent *keyEvent);
    void domKeyUp(nsIDOMKeyEvent *keyEvent);
    void domMouseDown(nsIDOMMouseEvent *mouseEvent);
    void domMouseUp(nsIDOMMouseEvent *mouseEvent);
    void domMouseClick(nsIDOMMouseEvent *mouseEvent);
    void domMouseDblClick(nsIDOMMouseEvent *mouseEvent);
    void domMouseOver(nsIDOMMouseEvent *mouseEvent);
    void domMouseOut(nsIDOMMouseEvent *mouseEvent);
    void domActivate(nsIDOMUIEvent *event);
    void domFocusIn(nsIDOMUIEvent *event);
    void domFocusOut(nsIDOMUIEvent *event);


    void startURIOpen(const QString &url, bool &abort);

protected:
    friend class EmbedEventListener;
    friend class EmbedContentListener;
    /**
     * return true if you want to stop the propagation
     * of the event. By default the events are being
     * propagated
     */

    virtual bool domKeyDownEvent(nsIDOMKeyEvent *keyEvent);
    virtual bool domKeyPressEvent(nsIDOMKeyEvent *keyEvent);
    virtual bool domKeyUpEvent(nsIDOMKeyEvent *keyEvent);

    virtual bool domMouseDownEvent(nsIDOMMouseEvent *mouseEvent);
    virtual bool domMouseUpEvent(nsIDOMMouseEvent *mouseEvent);
    virtual bool domMouseClickEvent(nsIDOMMouseEvent *mouseEvent);
    virtual bool domMouseDblClickEvent(nsIDOMMouseEvent *mouseEvent);
    virtual bool domMouseOverEvent(nsIDOMMouseEvent *mouseEvent);
    virtual bool domMouseOutEvent(nsIDOMMouseEvent *mouseEvent);

    virtual bool domActivateEvent(nsIDOMUIEvent *event);
    virtual bool domFocusInEvent(nsIDOMUIEvent *event);
    virtual bool domFocusOutEvent(nsIDOMUIEvent *event);


protected:
    friend class EmbedWindow;
    friend class EmbedWindowCreator;
    friend class EmbedProgress;
    friend class EmbedContextMenuListener;
    friend class EmbedStream;
    friend class QGeckoGlobals;
    void emitScriptStatus(const QString &str);
    void emitLinkStatus(const QString &str);
    void contentStateChanged();
    void contentFinishedLoading();

    bool isChrome() const;
    bool chromeLoaded() const;

protected:
    void resizeEvent(QResizeEvent *e);

    void setupListener();
    void attachListeners();

    EmbedWindow *window() const;
    int GetPIDOMWindow(nsPIDOMWindow **aPIWin);

protected:
    QGeckoEmbedPrivate *d;
};

#endif
