/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIURI.h"
#include "nsMaiHyperlink.h"
#include "mozilla/a11y/ProxyAccessible.h"

using namespace mozilla::a11y;

/* MaiAtkHyperlink */

#define MAI_TYPE_ATK_HYPERLINK      (mai_atk_hyperlink_get_type ())
#define MAI_ATK_HYPERLINK(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj),\
                                     MAI_TYPE_ATK_HYPERLINK, MaiAtkHyperlink))
#define MAI_ATK_HYPERLINK_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST ((klass),\
                                 MAI_TYPE_ATK_HYPERLINK, MaiAtkHyperlinkClass))
#define MAI_IS_ATK_HYPERLINK(obj)      (G_TYPE_CHECK_INSTANCE_TYPE ((obj),\
                                        MAI_TYPE_ATK_HYPERLINK))
#define MAI_IS_ATK_HYPERLINK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),\
                                        MAI_TYPE_ATK_HYPERLINK))
#define MAI_ATK_HYPERLINK_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj),\
                                 MAI_TYPE_ATK_HYPERLINK, MaiAtkHyperlinkClass))

/**
 * This MaiAtkHyperlink is a thin wrapper, in the MAI namespace,
 * for AtkHyperlink
 */

struct MaiAtkHyperlink
{
    AtkHyperlink parent;

    /*
     * The MaiHyperlink whose properties and features are exported via this
     * hyperlink instance.
     */
    MaiHyperlink *maiHyperlink;
};

struct MaiAtkHyperlinkClass
{
    AtkHyperlinkClass parent_class;
};

GType mai_atk_hyperlink_get_type(void);

G_BEGIN_DECLS
/* callbacks for AtkHyperlink */
static void classInitCB(AtkHyperlinkClass *aClass);
static void finalizeCB(GObject *aObj);

/* callbacks for AtkHyperlink virtual functions */
static gchar *getUriCB(AtkHyperlink *aLink, gint aLinkIndex);
static AtkObject *getObjectCB(AtkHyperlink *aLink, gint aLinkIndex);
static gint getEndIndexCB(AtkHyperlink *aLink);
static gint getStartIndexCB(AtkHyperlink *aLink);
static gboolean isValidCB(AtkHyperlink *aLink);
static gint getAnchorCountCB(AtkHyperlink *aLink);
G_END_DECLS

static gpointer parent_class = nullptr;

static MaiHyperlink*
GetMaiHyperlink(AtkHyperlink *aHyperlink)
{
    NS_ENSURE_TRUE(MAI_IS_ATK_HYPERLINK(aHyperlink), nullptr);
    MaiHyperlink * maiHyperlink =
        MAI_ATK_HYPERLINK(aHyperlink)->maiHyperlink;
    NS_ENSURE_TRUE(maiHyperlink != nullptr, nullptr);
    NS_ENSURE_TRUE(maiHyperlink->GetAtkHyperlink() == aHyperlink, nullptr);
    return maiHyperlink;
}

GType
mai_atk_hyperlink_get_type(void)
{
    static GType type = 0;

    if (!type) {
        static const GTypeInfo tinfo = {
            sizeof(MaiAtkHyperlinkClass),
            (GBaseInitFunc)nullptr,
            (GBaseFinalizeFunc)nullptr,
            (GClassInitFunc)classInitCB,
            (GClassFinalizeFunc)nullptr,
            nullptr, /* class data */
            sizeof(MaiAtkHyperlink), /* instance size */
            0, /* nb preallocs */
            (GInstanceInitFunc)nullptr,
            nullptr /* value table */
        };

        type = g_type_register_static(ATK_TYPE_HYPERLINK,
                                      "MaiAtkHyperlink",
                                      &tinfo, GTypeFlags(0));
    }
    return type;
}

MaiHyperlink::MaiHyperlink(uintptr_t aHyperLink) :
    mHyperlink(aHyperLink),
    mMaiAtkHyperlink(nullptr)
{
    mMaiAtkHyperlink =
        reinterpret_cast<AtkHyperlink *>
                        (g_object_new(mai_atk_hyperlink_get_type(), nullptr));
    NS_ASSERTION(mMaiAtkHyperlink, "OUT OF MEMORY");
    if (!mMaiAtkHyperlink)
      return;

    MAI_ATK_HYPERLINK(mMaiAtkHyperlink)->maiHyperlink = this;
}

MaiHyperlink::~MaiHyperlink()
{
  if (mMaiAtkHyperlink) {
    MAI_ATK_HYPERLINK(mMaiAtkHyperlink)->maiHyperlink = nullptr;
    g_object_unref(mMaiAtkHyperlink);
  }
}


/* static functions for ATK callbacks */

void
classInitCB(AtkHyperlinkClass *aClass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(aClass);

    parent_class = g_type_class_peek_parent(aClass);

    aClass->get_uri = getUriCB;
    aClass->get_object = getObjectCB;
    aClass->get_end_index = getEndIndexCB;
    aClass->get_start_index = getStartIndexCB;
    aClass->is_valid = isValidCB;
    aClass->get_n_anchors = getAnchorCountCB;

    gobject_class->finalize = finalizeCB;
}

void
finalizeCB(GObject *aObj)
{
    NS_ASSERTION(MAI_IS_ATK_HYPERLINK(aObj), "Invalid MaiAtkHyperlink");
    if (!MAI_IS_ATK_HYPERLINK(aObj))
        return;

    MaiAtkHyperlink *maiAtkHyperlink = MAI_ATK_HYPERLINK(aObj);
    maiAtkHyperlink->maiHyperlink = nullptr;

    /* call parent finalize function */
    if (G_OBJECT_CLASS (parent_class)->finalize)
        G_OBJECT_CLASS (parent_class)->finalize(aObj);
}

gchar *
getUriCB(AtkHyperlink *aLink, gint aLinkIndex)
{
  MaiHyperlink* maiLink = GetMaiHyperlink(aLink);
  if (!maiLink)
    return nullptr;

  nsAutoCString cautoStr;
  if (Accessible* hyperlink = maiLink->GetAccHyperlink()) {
    nsCOMPtr<nsIURI> uri = hyperlink->AnchorURIAt(aLinkIndex);
    if (!uri)
      return nullptr;

    nsresult rv = uri->GetSpec(cautoStr);
    NS_ENSURE_SUCCESS(rv, nullptr);

    return g_strdup(cautoStr.get());
  }

  bool valid;
  maiLink->Proxy()->AnchorURIAt(aLinkIndex, cautoStr, &valid);
  if (!valid)
    return nullptr;

  return g_strdup(cautoStr.get());
}

AtkObject *
getObjectCB(AtkHyperlink *aLink, gint aLinkIndex)
{
  MaiHyperlink* maiLink = GetMaiHyperlink(aLink);
  if (!maiLink)
    return nullptr;

    if (Accessible* hyperlink = maiLink->GetAccHyperlink()) {
      Accessible* anchor = hyperlink->AnchorAt(aLinkIndex);
      NS_ENSURE_TRUE(anchor, nullptr);

      return AccessibleWrap::GetAtkObject(anchor);
    }

    ProxyAccessible* anchor = maiLink->Proxy()->AnchorAt(aLinkIndex);
    return anchor ? GetWrapperFor(anchor) : nullptr;
}

gint
getEndIndexCB(AtkHyperlink *aLink)
{
  MaiHyperlink* maiLink = GetMaiHyperlink(aLink);
  if (!maiLink)
    return false;

  if (Accessible* hyperlink = maiLink->GetAccHyperlink())
    return static_cast<gint>(hyperlink->EndOffset());

  bool valid = false;
  uint32_t endIdx = maiLink->Proxy()->EndOffset(&valid);
  return valid ? static_cast<gint>(endIdx) : -1;
}

gint
getStartIndexCB(AtkHyperlink *aLink)
{
  MaiHyperlink* maiLink = GetMaiHyperlink(aLink);
  if (!maiLink)
    return -1;

  if (Accessible* hyperlink = maiLink->GetAccHyperlink())
    return static_cast<gint>(hyperlink->StartOffset());

  bool valid = false;
  uint32_t startIdx = maiLink->Proxy()->StartOffset(&valid);
  return valid ? static_cast<gint>(startIdx) : -1;
}

gboolean
isValidCB(AtkHyperlink *aLink)
{
  MaiHyperlink* maiLink = GetMaiHyperlink(aLink);
  if (!maiLink)
    return false;

  if (Accessible* hyperlink = maiLink->GetAccHyperlink())
    return static_cast<gboolean>(hyperlink->IsLinkValid());

  return static_cast<gboolean>(maiLink->Proxy()->IsLinkValid());
}

gint
getAnchorCountCB(AtkHyperlink *aLink)
{
  MaiHyperlink* maiLink = GetMaiHyperlink(aLink);
  if (!maiLink)
    return -1;

  if (Accessible* hyperlink = maiLink->GetAccHyperlink())
    return static_cast<gint>(hyperlink->AnchorCount());

  bool valid = false;
  uint32_t anchorCount = maiLink->Proxy()->AnchorCount(&valid);
  return valid ? static_cast<gint>(anchorCount) : -1;
}
