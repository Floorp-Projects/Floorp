/* ***** BEGIN LICENSE BLOCK *****
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
 * David Hyatt (hyatt@netscape.com).
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Lars Knoll <knoll@kde.org>
 *   Zack Rusin <zack@kde.org>
 *   Tim Hill (tim@prismelite.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsNativeThemeQt.h"
#include "nsRenderingContextQt.h"
#include "nsDeviceContextQt.h"
#include "nsRect.h"
#include "nsSize.h"
#include "nsTransform2D.h"
#include "nsThemeConstants.h"
#include "nsILookAndFeel.h"
#include "nsIServiceManager.h"
#include "nsIEventStateManager.h"
#include <malloc.h>

#include <qapplication.h>
#include <qstyle.h>
#include <qpalette.h>
#include <qcombobox.h>

nsNativeThemeQt::nsNativeThemeQt()
{
    combo = new QComboBox((QWidget *)0);
    combo->resize(0, 0);
    ThemeChanged();
}

nsNativeThemeQt::~nsNativeThemeQt()
{
    delete combo;
}

NS_IMPL_ISUPPORTS1(nsNativeThemeQt, nsITheme)

NS_IMETHODIMP
nsNativeThemeQt::DrawWidgetBackground(nsIRenderingContext* aContext,
                                      nsIFrame* aFrame,
                                      PRUint8 aWidgetType,
                                      const nsRect& aRect,
                                      const nsRect& aClipRect)
{
    nsRenderingContextQt *context = (nsRenderingContextQt *)aContext;
    QPainter *p = context->painter();

    //qDebug("aWidgetType = %d", aWidgetType);
    if (!p)
        return NS_OK;

    QStyle &s = qApp->style();
    const QColorGroup &cg = qApp->palette().active();

    QRect r = context->qRect(aRect);
    QRect cr = context->qRect(aClipRect);
    context->UpdateGC();
    //qDebug("rect=%d %d %d %d", aRect.x, aRect.y, aRect.width, aRect.height);
    p->save();
    p->translate(r.x(), r.y());
    r.moveBy(-r.x(), -r.y());

    QStyle::PrimitiveElement pe = QStyle::PE_CustomBase;
    QStyle::SFlags flags =  IsDisabled(aFrame) ?
                            QStyle::Style_Default :
                            QStyle::Style_Enabled;

    PRInt32 eventState = GetContentState(aFrame, aWidgetType);
    //qDebug("eventState = %d",  eventState);

    if (eventState & NS_EVENT_STATE_HOVER)
        flags |= QStyle::Style_MouseOver;
    if (eventState & NS_EVENT_STATE_FOCUS)
        flags |= QStyle::Style_HasFocus;
    if (eventState & NS_EVENT_STATE_ACTIVE)
        flags |= QStyle::Style_Down;

    switch (aWidgetType) {
    case NS_THEME_RADIO:
        flags |= (IsChecked(aFrame) ? QStyle::Style_On : QStyle::Style_Off);
        pe = QStyle::PE_ExclusiveIndicator;
        break;
    case NS_THEME_CHECKBOX:
        flags |= (IsChecked(aFrame) ? QStyle::Style_On : QStyle::Style_Off);
        pe = QStyle::PE_Indicator;
        break;
    case NS_THEME_SCROLLBAR:
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
        p->fillRect(r, qApp->palette().brush(QPalette::Active, QColorGroup::Background));
        break;
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
        flags |= QStyle::Style_Horizontal;
        // fall through
    case NS_THEME_SCROLLBAR_BUTTON_UP:
        pe = QStyle::PE_ScrollBarSubLine;
        break;
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
        flags |= QStyle::Style_Horizontal;
        // fall through
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
        pe = QStyle::PE_ScrollBarAddLine;
        break;
    case NS_THEME_SCROLLBAR_GRIPPER_HORIZONTAL:
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
        flags |= QStyle::Style_Horizontal;
        // fall through
    case NS_THEME_SCROLLBAR_GRIPPER_VERTICAL:
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
        pe = QStyle::PE_ScrollBarSlider;
        break;
    case NS_THEME_BUTTON_BEVEL:
        pe = QStyle::PE_ButtonBevel;
        flags |= QStyle::Style_Raised;
        break;
    case NS_THEME_BUTTON:
	pe = IsDefaultButton(aFrame) ? QStyle::PE_ButtonDefault : QStyle::PE_ButtonCommand;
        flags |= QStyle::Style_Raised;
        break;
    case NS_THEME_DROPDOWN:
        s.drawComplexControl(QStyle::CC_ComboBox, p, combo, r, cg, flags, QStyle::SC_ComboBoxFrame);
        break;
    case NS_THEME_DROPDOWN_BUTTON:
        r.moveBy(frameWidth, -frameWidth);
        r.setHeight(r.height() + 2*frameWidth);
        s.drawComplexControl(QStyle::CC_ComboBox, p, combo, r, cg, flags, QStyle::SC_ComboBoxArrow);
        break;
    case NS_THEME_DROPDOWN_TEXT:
    case NS_THEME_DROPDOWN_TEXTFIELD:
        break;
    case NS_THEME_TEXTFIELD:
    case NS_THEME_LISTBOX:
        pe = QStyle::PE_PanelLineEdit;
        break;
    default:
        break;
    }
    if (pe != QStyle::PE_CustomBase)
        s.drawPrimitive(pe, p, r, cg, flags);
    p->restore();
    return NS_OK;
}

NS_IMETHODIMP
nsNativeThemeQt::GetWidgetBorder(nsIDeviceContext* aContext,
                                 nsIFrame* aFrame,
                                 PRUint8 aWidgetType,
                                 nsMargin* aResult)
{
    (*aResult).top = (*aResult).bottom = (*aResult).left = (*aResult).right = 0;

    switch(aWidgetType) {
    case NS_THEME_TEXTFIELD:
    case NS_THEME_LISTBOX:
        (*aResult).top = (*aResult).bottom = (*aResult).left = (*aResult).right =
                         frameWidth;
    }

    return NS_OK;
}

PRBool
nsNativeThemeQt::GetWidgetPadding(nsIDeviceContext* ,
                                  nsIFrame*, PRUint8 aWidgetType,
                                  nsMargin* aResult)
{
    //XXX: is this really correct?
    if (aWidgetType == NS_THEME_BUTTON_FOCUS ||
        aWidgetType == NS_THEME_TOOLBAR_BUTTON ||
        aWidgetType == NS_THEME_TOOLBAR_DUAL_BUTTON) {
        aResult->SizeTo(0, 0, 0, 0);
        return PR_TRUE;
    }

    return PR_FALSE;
}

NS_IMETHODIMP
nsNativeThemeQt::GetMinimumWidgetSize(nsIRenderingContext* aContext, nsIFrame* aFrame,
                                      PRUint8 aWidgetType,
                                      nsSize* aResult, PRBool* aIsOverridable)
{
    (*aResult).width = (*aResult).height = 0;
    *aIsOverridable = PR_TRUE;

    QStyle &s = qApp->style();

    switch (aWidgetType) {
    case NS_THEME_RADIO:
    case NS_THEME_CHECKBOX: {
        QRect rect = s.subRect(aWidgetType == NS_THEME_CHECKBOX
                               ? QStyle::SR_CheckBoxIndicator
                               : QStyle::SR_RadioButtonIndicator,
                               0);
        (*aResult).width = rect.width();
        (*aResult).height = rect.height();
        break;
    }
    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
        (*aResult).width = s.pixelMetric(QStyle::PM_ScrollBarExtent);
        (*aResult).height = (*aResult).width;
        *aIsOverridable = PR_FALSE;
        break;
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
        (*aResult).width = s.pixelMetric(QStyle::PM_ScrollBarExtent);
        (*aResult).height = s.pixelMetric(QStyle::PM_ScrollBarSliderMin);
        *aIsOverridable = PR_FALSE;
        break;
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
        (*aResult).width = s.pixelMetric(QStyle::PM_ScrollBarSliderMin);
        (*aResult).height = s.pixelMetric(QStyle::PM_ScrollBarExtent);
        *aIsOverridable = PR_FALSE;
        break;
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
        break;
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
        break;
    case NS_THEME_DROPDOWN_BUTTON: {
        QRect r = s.querySubControlMetrics(QStyle::CC_ComboBox, combo, QStyle::SC_ComboBoxArrow);
        (*aResult).width = r.width() - 2*frameWidth;
        (*aResult).height = r.height() - 2*frameWidth;
        break;
    }
    case NS_THEME_DROPDOWN:
    case NS_THEME_DROPDOWN_TEXT:
    case NS_THEME_DROPDOWN_TEXTFIELD:
    case NS_THEME_TEXTFIELD:
        break;
    }
    return NS_OK;
}

    NS_IMETHODIMP
    nsNativeThemeQt::WidgetStateChanged(nsIFrame* aFrame, PRUint8 aWidgetType,
                                        nsIAtom* aAttribute, PRBool* aShouldRepaint)
{
    *aShouldRepaint = TRUE;
    return NS_OK;
}


NS_IMETHODIMP
nsNativeThemeQt::ThemeChanged()
{
    QStyle & s = qApp->style();
    frameWidth = s.pixelMetric(QStyle::PM_DefaultFrameWidth);
    return NS_OK;
}

PRBool
nsNativeThemeQt::ThemeSupportsWidget(nsPresContext* aPresContext,
                                     nsIFrame* aFrame,
                                     PRUint8 aWidgetType)
{
    switch (aWidgetType) {
    case NS_THEME_SCROLLBAR:
    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
    case NS_THEME_SCROLLBAR_GRIPPER_HORIZONTAL:
    case NS_THEME_SCROLLBAR_GRIPPER_VERTICAL:
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
    case NS_THEME_RADIO:
    case NS_THEME_CHECKBOX:
    case NS_THEME_BUTTON_BEVEL:
    case NS_THEME_BUTTON:
    case NS_THEME_DROPDOWN:
    case NS_THEME_DROPDOWN_BUTTON:
    case NS_THEME_DROPDOWN_TEXT:
    case NS_THEME_DROPDOWN_TEXTFIELD:
    case NS_THEME_TEXTFIELD:
    case NS_THEME_LISTBOX:
        return PR_TRUE;
    default:
        break;
    }
    return PR_FALSE;
}

PRBool
nsNativeThemeQt::WidgetIsContainer(PRUint8 aWidgetType)
{
    // XXXdwh At some point flesh all of this out.
    if (aWidgetType == NS_THEME_DROPDOWN_BUTTON ||
        aWidgetType == NS_THEME_RADIO ||
        aWidgetType == NS_THEME_CHECKBOX)
        return PR_FALSE;
    return PR_TRUE;
}
