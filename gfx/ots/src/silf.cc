// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "silf.h"

#include "name.h"
#include "mozilla/Compression.h"
#include <cmath>

namespace ots {

bool OpenTypeSILF::Parse(const uint8_t* data, size_t length,
                         bool prevent_decompression) {
  if (GetFont()->dropped_graphite) {
    return Drop("Skipping Graphite table");
  }
  Buffer table(data, length);

  if (!table.ReadU32(&this->version)) {
    return DropGraphite("Failed to read version");
  }
  if (this->version >> 16 != 1 &&
      this->version >> 16 != 2 &&
      this->version >> 16 != 3 &&
      this->version >> 16 != 4 &&
      this->version >> 16 != 5) {
    return DropGraphite("Unsupported table version: %u", this->version >> 16);
  }
  if (this->version >> 16 >= 3 && !table.ReadU32(&this->compHead)) {
    return DropGraphite("Failed to read compHead");
  }
  if (this->version >> 16 >= 5) {
    switch ((this->compHead & SCHEME) >> 27) {
      case 0:  // uncompressed
        break;
      case 1: {  // lz4
        if (prevent_decompression) {
          return DropGraphite("Illegal nested compression");
        }
        std::vector<uint8_t> decompressed(this->compHead & FULL_SIZE);
        size_t outputSize = 0;
        bool ret = mozilla::Compression::LZ4::decompressPartial(
            reinterpret_cast<const char*>(data + table.offset()),
            table.remaining(),  // input buffer size (input size + padding)
            reinterpret_cast<char*>(decompressed.data()),
            decompressed.size(),  // target output size
            &outputSize);   // return output size
        if (!ret || outputSize != decompressed.size()) {
          return DropGraphite("Decompression failed");
        }
        return this->Parse(decompressed.data(), decompressed.size(), true);
      }
      default:
        return DropGraphite("Unknown compression scheme");
    }
  }
  if (!table.ReadU16(&this->numSub)) {
    return DropGraphite("Failed to read numSub");
  }
  if (this->version >> 16 >= 2 && !table.ReadU16(&this->reserved)) {
    return DropGraphite("Failed to read reserved");
  }
  if (this->version >> 16 >= 2 && this->reserved != 0) {
    Warning("Nonzero reserved");
  }

  unsigned long last_offset = 0;
  //this->offset.resize(this->numSub);
  for (unsigned i = 0; i < this->numSub; ++i) {
    this->offset.emplace_back();
    if (!table.ReadU32(&this->offset[i]) || this->offset[i] < last_offset) {
      return DropGraphite("Failed to read offset[%u]", i);
    }
    last_offset = this->offset[i];
  }

  for (unsigned i = 0; i < this->numSub; ++i) {
    if (table.offset() != this->offset[i]) {
      return DropGraphite("Offset check failed for tables[%lu]", i);
    }
    SILSub subtable(this);
    if (!subtable.ParsePart(table)) {
      return DropGraphite("Failed to read tables[%u]", i);
    }
    tables.push_back(subtable);
  }

  if (table.remaining()) {
    return Warning("%zu bytes unparsed", table.remaining());
  }
  return true;
}

bool OpenTypeSILF::Serialize(OTSStream* out) {
  if (!out->WriteU32(this->version) ||
      (this->version >> 16 >= 3 && !out->WriteU32(this->compHead)) ||
      !out->WriteU16(this->numSub) ||
      (this->version >> 16 >= 2 && !out->WriteU16(this->reserved)) ||
      !SerializeParts(this->offset, out) ||
      !SerializeParts(this->tables, out)) {
    return Error("Failed to write table");
  }
  return true;
}

bool OpenTypeSILF::SILSub::ParsePart(Buffer& table) {
  size_t init_offset = table.offset();
  if (parent->version >> 16 >= 3) {
    if (!table.ReadU32(&this->ruleVersion)) {
      return parent->Error("SILSub: Failed to read ruleVersion");
    }
    if (!table.ReadU16(&this->passOffset)) {
      return parent->Error("SILSub: Failed to read passOffset");
    }
    if (!table.ReadU16(&this->pseudosOffset)) {
      return parent->Error("SILSub: Failed to read pseudosOffset");
    }
  }
  if (!table.ReadU16(&this->maxGlyphID)) {
    return parent->Error("SILSub: Failed to read maxGlyphID");
  }
  if (!table.ReadS16(&this->extraAscent)) {
    return parent->Error("SILSub: Failed to read extraAscent");
  }
  if (!table.ReadS16(&this->extraDescent)) {
    return parent->Error("SILSub: Failed to read extraDescent");
  }
  if (!table.ReadU8(&this->numPasses)) {
    return parent->Error("SILSub: Failed to read numPasses");
  }
  if (!table.ReadU8(&this->iSubst) || this->iSubst > this->numPasses) {
    return parent->Error("SILSub: Failed to read valid iSubst");
  }
  if (!table.ReadU8(&this->iPos) || this->iPos > this->numPasses) {
    return parent->Error("SILSub: Failed to read valid iPos");
  }
  if (!table.ReadU8(&this->iJust) || this->iJust > this->numPasses) {
    return parent->Error("SILSub: Failed to read valid iJust");
  }
  if (!table.ReadU8(&this->iBidi) ||
      !(iBidi == 0xFF || this->iBidi <= this->iPos)) {
    return parent->Error("SILSub: Failed to read valid iBidi");
  }
  if (!table.ReadU8(&this->flags)) {
    return parent->Error("SILSub: Failed to read flags");
    // checks omitted
  }
  if (!table.ReadU8(&this->maxPreContext)) {
    return parent->Error("SILSub: Failed to read maxPreContext");
  }
  if (!table.ReadU8(&this->maxPostContext)) {
    return parent->Error("SILSub: Failed to read maxPostContext");
  }
  if (!table.ReadU8(&this->attrPseudo)) {
    return parent->Error("SILSub: Failed to read attrPseudo");
  }
  if (!table.ReadU8(&this->attrBreakWeight)) {
    return parent->Error("SILSub: Failed to read attrBreakWeight");
  }
  if (!table.ReadU8(&this->attrDirectionality)) {
    return parent->Error("SILSub: Failed to read attrDirectionality");
  }
  if (parent->version >> 16 >= 2) {
    if (!table.ReadU8(&this->attrMirroring)) {
      return parent->Error("SILSub: Failed to read attrMirroring");
    }
    if (parent->version >> 16 < 4 && this->attrMirroring != 0) {
      parent->Warning("SILSub: Nonzero attrMirroring (reserved before v4)");
    }
    if (!table.ReadU8(&this->attrSkipPasses)) {
      return parent->Error("SILSub: Failed to read attrSkipPasses");
    }
    if (parent->version >> 16 < 4 && this->attrSkipPasses != 0) {
      parent->Warning("SILSub: Nonzero attrSkipPasses (reserved2 before v4)");
    }

    if (!table.ReadU8(&this->numJLevels)) {
      return parent->Error("SILSub: Failed to read numJLevels");
    }
    //this->jLevels.resize(this->numJLevels, parent);
    for (unsigned i = 0; i < this->numJLevels; ++i) {
      this->jLevels.emplace_back(parent);
      if (!this->jLevels[i].ParsePart(table)) {
        return parent->Error("SILSub: Failed to read jLevels[%u]", i);
      }
    }
  }

  if (!table.ReadU16(&this->numLigComp)) {
    return parent->Error("SILSub: Failed to read numLigComp");
  }
  if (!table.ReadU8(&this->numUserDefn)) {
    return parent->Error("SILSub: Failed to read numUserDefn");
  }
  if (!table.ReadU8(&this->maxCompPerLig)) {
    return parent->Error("SILSub: Failed to read maxCompPerLig");
  }
  if (!table.ReadU8(&this->direction)) {
    return parent->Error("SILSub: Failed to read direction");
  }
  if (!table.ReadU8(&this->attCollisions)) {
    return parent->Error("SILSub: Failed to read attCollisions");
  }
  if (parent->version >> 16 < 5 && this->attCollisions != 0) {
    parent->Warning("SILSub: Nonzero attCollisions (reserved before v5)");
  }
  if (!table.ReadU8(&this->reserved4)) {
    return parent->Error("SILSub: Failed to read reserved4");
  }
  if (this->reserved4 != 0) {
    parent->Warning("SILSub: Nonzero reserved4");
  }
  if (!table.ReadU8(&this->reserved5)) {
    return parent->Error("SILSub: Failed to read reserved5");
  }
  if (this->reserved5 != 0) {
    parent->Warning("SILSub: Nonzero reserved5");
  }
  if (parent->version >> 16 >= 2) {
    if (!table.ReadU8(&this->reserved6)) {
      return parent->Error("SILSub: Failed to read reserved6");
    }
    if (this->reserved6 != 0) {
      parent->Warning("SILSub: Nonzero reserved6");
    }

    if (!table.ReadU8(&this->numCritFeatures)) {
      return parent->Error("SILSub: Failed to read numCritFeatures");
    }
    //this->critFeatures.resize(this->numCritFeatures);
    for (unsigned i = 0; i < this->numCritFeatures; ++i) {
      this->critFeatures.emplace_back();
      if (!table.ReadU16(&this->critFeatures[i])) {
        return parent->Error("SILSub: Failed to read critFeatures[%u]", i);
      }
    }

    if (!table.ReadU8(&this->reserved7)) {
      return parent->Error("SILSub: Failed to read reserved7");
    }
    if (this->reserved7 != 0) {
      parent->Warning("SILSub: Nonzero reserved7");
    }
  }

  if (!table.ReadU8(&this->numScriptTag)) {
    return parent->Error("SILSub: Failed to read numScriptTag");
  }
  //this->scriptTag.resize(this->numScriptTag);
  for (unsigned i = 0; i < this->numScriptTag; ++i) {
    this->scriptTag.emplace_back();
    if (!table.ReadU32(&this->scriptTag[i])) {
      return parent->Error("SILSub: Failed to read scriptTag[%u]", i);
    }
  }

  if (!table.ReadU16(&this->lbGID)) {
    return parent->Error("SILSub: Failed to read lbGID");
  }
  if (this->lbGID > this->maxGlyphID) {
    parent->Warning("SILSub: lbGID %u outside range 0..%u, replaced with 0",
                    this->lbGID, this->maxGlyphID);
    this->lbGID = 0;
  }

  if (parent->version >> 16 >= 3 &&
      table.offset() != init_offset + this->passOffset) {
    return parent->Error("SILSub: passOffset check failed");
  }
  unsigned long last_oPass = 0;
  //this->oPasses.resize(static_cast<unsigned>(this->numPasses) + 1);
  for (unsigned i = 0; i <= this->numPasses; ++i) {
    this->oPasses.emplace_back();
    if (!table.ReadU32(&this->oPasses[i]) || this->oPasses[i] < last_oPass) {
      return false;
    }
    last_oPass = this->oPasses[i];
  }

  if (parent->version >> 16 >= 3 &&
      table.offset() != init_offset + this->pseudosOffset) {
    return parent->Error("SILSub: pseudosOffset check failed");
  }
  if (!table.ReadU16(&this->numPseudo)) {
    return parent->Error("SILSub: Failed to read numPseudo");
  }

  // The following three fields are deprecated and ignored. We fix them up here
  // just for internal consistency, but the Graphite engine doesn't care.
  if (!table.ReadU16(&this->searchPseudo) ||
      !table.ReadU16(&this->pseudoSelector) ||
      !table.ReadU16(&this->pseudoShift)) {
    return parent->Error("SILSub: Failed to read searchPseudo..pseudoShift");
  }
  if (this->numPseudo == 0) {
    if (this->searchPseudo != 0 || this->pseudoSelector != 0 || this->pseudoShift != 0) {
      this->searchPseudo = this->pseudoSelector = this->pseudoShift = 0;
    }
  } else {
    unsigned floorLog2 = std::floor(std::log2(this->numPseudo));
    if (this->searchPseudo != 6 * (unsigned)std::pow(2, floorLog2) ||
        this->pseudoSelector != floorLog2 ||
        this->pseudoShift != 6 * this->numPseudo - this->searchPseudo) {
      this->searchPseudo = 6 * (unsigned)std::pow(2, floorLog2);
      this->pseudoSelector = floorLog2;
      this->pseudoShift = 6 * this->numPseudo - this->searchPseudo;
    }
  }

  //this->pMaps.resize(this->numPseudo, parent);
  for (unsigned i = 0; i < numPseudo; i++) {
    this->pMaps.emplace_back(parent);
    if (!this->pMaps[i].ParsePart(table)) {
      return parent->Error("SILSub: Failed to read pMaps[%u]", i);
    }
  }

  if (!this->classes.ParsePart(table)) {
    return parent->Error("SILSub: Failed to read classes");
  }

  //this->passes.resize(this->numPasses, parent);
  for (unsigned i = 0; i < this->numPasses; ++i) {
    this->passes.emplace_back(parent);
    if (table.offset() != init_offset + this->oPasses[i]) {
      return parent->Error("SILSub: Offset check failed for passes[%u]", i);
    }
    if (!this->passes[i].ParsePart(table, init_offset, this->oPasses[i+1])) {
      return parent->Error("SILSub: Failed to read passes[%u]", i);
    }
  }
  return true;
}

bool OpenTypeSILF::SILSub::SerializePart(OTSStream* out) const {
  if ((parent->version >> 16 >= 3 &&
       (!out->WriteU32(this->ruleVersion) ||
        !out->WriteU16(this->passOffset) ||
        !out->WriteU16(this->pseudosOffset))) ||
      !out->WriteU16(this->maxGlyphID) ||
      !out->WriteS16(this->extraAscent) ||
      !out->WriteS16(this->extraDescent) ||
      !out->WriteU8(this->numPasses) ||
      !out->WriteU8(this->iSubst) ||
      !out->WriteU8(this->iPos) ||
      !out->WriteU8(this->iJust) ||
      !out->WriteU8(this->iBidi) ||
      !out->WriteU8(this->flags) ||
      !out->WriteU8(this->maxPreContext) ||
      !out->WriteU8(this->maxPostContext) ||
      !out->WriteU8(this->attrPseudo) ||
      !out->WriteU8(this->attrBreakWeight) ||
      !out->WriteU8(this->attrDirectionality) ||
      (parent->version >> 16 >= 2 &&
       (!out->WriteU8(this->attrMirroring) ||
        !out->WriteU8(this->attrSkipPasses) ||
        !out->WriteU8(this->numJLevels) ||
        !SerializeParts(this->jLevels, out))) ||
      !out->WriteU16(this->numLigComp) ||
      !out->WriteU8(this->numUserDefn) ||
      !out->WriteU8(this->maxCompPerLig) ||
      !out->WriteU8(this->direction) ||
      !out->WriteU8(this->attCollisions) ||
      !out->WriteU8(this->reserved4) ||
      !out->WriteU8(this->reserved5) ||
      (parent->version >> 16 >= 2 &&
       (!out->WriteU8(this->reserved6) ||
        !out->WriteU8(this->numCritFeatures) ||
        !SerializeParts(this->critFeatures, out) ||
        !out->WriteU8(this->reserved7))) ||
      !out->WriteU8(this->numScriptTag) ||
      !SerializeParts(this->scriptTag, out) ||
      !out->WriteU16(this->lbGID) ||
      !SerializeParts(this->oPasses, out) ||
      !out->WriteU16(this->numPseudo) ||
      !out->WriteU16(this->searchPseudo) ||
      !out->WriteU16(this->pseudoSelector) ||
      !out->WriteU16(this->pseudoShift) ||
      !SerializeParts(this->pMaps, out) ||
      !this->classes.SerializePart(out) ||
      !SerializeParts(this->passes, out)) {
    return parent->Error("SILSub: Failed to write");
  }
  return true;
}

bool OpenTypeSILF::SILSub::
JustificationLevel::ParsePart(Buffer& table) {
  if (!table.ReadU8(&this->attrStretch)) {
    return parent->Error("JustificationLevel: Failed to read attrStretch");
  }
  if (!table.ReadU8(&this->attrShrink)) {
    return parent->Error("JustificationLevel: Failed to read attrShrink");
  }
  if (!table.ReadU8(&this->attrStep)) {
    return parent->Error("JustificationLevel: Failed to read attrStep");
  }
  if (!table.ReadU8(&this->attrWeight)) {
    return parent->Error("JustificationLevel: Failed to read attrWeight");
  }
  if (!table.ReadU8(&this->runto)) {
    return parent->Error("JustificationLevel: Failed to read runto");
  }
  if (!table.ReadU8(&this->reserved)) {
    return parent->Error("JustificationLevel: Failed to read reserved");
  }
  if (this->reserved != 0) {
    parent->Warning("JustificationLevel: Nonzero reserved");
  }
  if (!table.ReadU8(&this->reserved2)) {
    return parent->Error("JustificationLevel: Failed to read reserved2");
  }
  if (this->reserved2 != 0) {
    parent->Warning("JustificationLevel: Nonzero reserved2");
  }
  if (!table.ReadU8(&this->reserved3)) {
    return parent->Error("JustificationLevel: Failed to read reserved3");
  }
  if (this->reserved3 != 0) {
    parent->Warning("JustificationLevel: Nonzero reserved3");
  }
  return true;
}

bool OpenTypeSILF::SILSub::
JustificationLevel::SerializePart(OTSStream* out) const {
  if (!out->WriteU8(this->attrStretch) ||
      !out->WriteU8(this->attrShrink) ||
      !out->WriteU8(this->attrStep) ||
      !out->WriteU8(this->attrWeight) ||
      !out->WriteU8(this->runto) ||
      !out->WriteU8(this->reserved) ||
      !out->WriteU8(this->reserved2) ||
      !out->WriteU8(this->reserved3)) {
    return parent->Error("JustificationLevel: Failed to write");
  }
  return true;
}

bool OpenTypeSILF::SILSub::
PseudoMap::ParsePart(Buffer& table) {
  if (parent->version >> 16 >= 2 && !table.ReadU32(&this->unicode)) {
    return parent->Error("PseudoMap: Failed to read unicode");
  }
  if (parent->version >> 16 == 1) {
    uint16_t unicode;
    if (!table.ReadU16(&unicode)) {
      return parent->Error("PseudoMap: Failed to read unicode");
    }
    this->unicode = unicode;
  }
  if (!table.ReadU16(&this->nPseudo)) {
    return parent->Error("PseudoMap: Failed to read nPseudo");
  }
  return true;
}

bool OpenTypeSILF::SILSub::
PseudoMap::SerializePart(OTSStream* out) const {
  if ((parent->version >> 16 >= 2 && !out->WriteU32(this->unicode)) ||
      (parent->version >> 16 == 1 &&
       !out->WriteU16(static_cast<uint16_t>(this->unicode))) ||
      !out->WriteU16(this->nPseudo)) {
    return parent->Error("PseudoMap: Failed to write");
  }
  return true;
}

bool OpenTypeSILF::SILSub::
ClassMap::ParsePart(Buffer& table) {
  size_t init_offset = table.offset();
  if (!table.ReadU16(&this->numClass)) {
    return parent->Error("ClassMap: Failed to read numClass");
  }
  if (!table.ReadU16(&this->numLinear) || this->numLinear > this->numClass) {
    return parent->Error("ClassMap: Failed to read valid numLinear");
  }

  //this->oClass.resize(static_cast<unsigned long>(this->numClass) + 1);
  if (parent->version >> 16 >= 4) {
    unsigned long last_oClass = 0;
    for (unsigned long i = 0; i <= this->numClass; ++i) {
      this->oClass.emplace_back();
      if (!table.ReadU32(&this->oClass[i]) || this->oClass[i] < last_oClass) {
        return parent->Error("ClassMap: Failed to read oClass[%lu]", i);
      }
      last_oClass = this->oClass[i];
    }
  }
  if (parent->version >> 16 < 4) {
    unsigned last_oClass = 0;
    for (unsigned long i = 0; i <= this->numClass; ++i) {
      uint16_t offset;
      if (!table.ReadU16(&offset) || offset < last_oClass) {
        return parent->Error("ClassMap: Failed to read oClass[%lu]", i);
      }
      last_oClass = offset;
      this->oClass.push_back(static_cast<uint32_t>(offset));
    }
  }

  if (table.offset() - init_offset > this->oClass[this->numLinear]) {
    return parent->Error("ClassMap: Failed to calculate length of glyphs");
  }
  unsigned long glyphs_len = (this->oClass[this->numLinear] -
                             (table.offset() - init_offset))/2;
  //this->glyphs.resize(glyphs_len);
  for (unsigned long i = 0; i < glyphs_len; ++i) {
    this->glyphs.emplace_back();
    if (!table.ReadU16(&this->glyphs[i])) {
      return parent->Error("ClassMap: Failed to read glyphs[%lu]", i);
    }
  }

  unsigned lookups_len = this->numClass - this->numLinear;
    // this->numLinear <= this->numClass
  //this->lookups.resize(lookups_len, parent);
  for (unsigned i = 0; i < lookups_len; ++i) {
    this->lookups.emplace_back(parent);
    if (table.offset() != init_offset + oClass[this->numLinear + i]) {
      return parent->Error("ClassMap: Offset check failed for lookups[%u]", i);
    }
    if (!this->lookups[i].ParsePart(table)) {
      return parent->Error("ClassMap: Failed to read lookups[%u]", i);
    }
  }
  return true;
}

bool OpenTypeSILF::SILSub::
ClassMap::SerializePart(OTSStream* out) const {
  if (!out->WriteU16(this->numClass) ||
      !out->WriteU16(this->numLinear) ||
      (parent->version >> 16 >= 4 && !SerializeParts(this->oClass, out)) ||
      (parent->version >> 16 < 4 &&
       ![&] {
         for (uint32_t offset : this->oClass) {
           if (!out->WriteU16(static_cast<uint16_t>(offset))) {
             return false;
           }
         }
         return true;
       }()) ||
      !SerializeParts(this->glyphs, out) ||
      !SerializeParts(this->lookups, out)) {
    return parent->Error("ClassMap: Failed to write");
  }
  return true;
}

bool OpenTypeSILF::SILSub::ClassMap::
LookupClass::ParsePart(Buffer& table) {
  if (!table.ReadU16(&this->numIDs)) {
    return parent->Error("LookupClass: Failed to read numIDs");
  }
  if (!table.ReadU16(&this->searchRange) ||
      !table.ReadU16(&this->entrySelector) ||
      !table.ReadU16(&this->rangeShift)) {
    return parent->Error("LookupClass: Failed to read searchRange..rangeShift");
  }
  if (this->numIDs == 0) {
    if (this->searchRange != 0 || this->entrySelector != 0 || this->rangeShift != 0) {
      parent->Warning("LookupClass: Correcting binary-search header for zero-length LookupPair list");
      this->searchRange = this->entrySelector = this->rangeShift = 0;
    }
  } else {
    unsigned floorLog2 = std::floor(std::log2(this->numIDs));
    if (this->searchRange != (unsigned)std::pow(2, floorLog2) ||
        this->entrySelector != floorLog2 ||
        this->rangeShift != this->numIDs - this->searchRange) {
      parent->Warning("LookupClass: Correcting binary-search header for LookupPair list");
      this->searchRange = (unsigned)std::pow(2, floorLog2);
      this->entrySelector = floorLog2;
      this->rangeShift = this->numIDs - this->searchRange;
    }
  }

  //this->lookups.resize(this->numIDs, parent);
  for (unsigned i = 0; i < numIDs; ++i) {
    this->lookups.emplace_back(parent);
    if (!this->lookups[i].ParsePart(table)) {
      return parent->Error("LookupClass: Failed to read lookups[%u]", i);
    }
  }
  return true;
}

bool OpenTypeSILF::SILSub::ClassMap::
LookupClass::SerializePart(OTSStream* out) const {
  if (!out->WriteU16(this->numIDs) ||
      !out->WriteU16(this->searchRange) ||
      !out->WriteU16(this->entrySelector) ||
      !out->WriteU16(this->rangeShift) ||
      !SerializeParts(this->lookups, out)) {
    return parent->Error("LookupClass: Failed to write");
  }
  return true;
}

bool OpenTypeSILF::SILSub::ClassMap::LookupClass::
LookupPair::ParsePart(Buffer& table) {
  if (!table.ReadU16(&this->glyphId)) {
    return parent->Error("LookupPair: Failed to read glyphId");
  }
  if (!table.ReadU16(&this->index)) {
    return parent->Error("LookupPair: Failed to read index");
  }
  return true;
}

bool OpenTypeSILF::SILSub::ClassMap::LookupClass::
LookupPair::SerializePart(OTSStream* out) const {
  if (!out->WriteU16(this->glyphId) ||
      !out->WriteU16(this->index)) {
    return parent->Error("LookupPair: Failed to write");
  }
  return true;
}

bool OpenTypeSILF::SILSub::
SILPass::ParsePart(Buffer& table, const size_t SILSub_init_offset,
                                  const size_t next_pass_offset) {
  size_t init_offset = table.offset();
  if (!table.ReadU8(&this->flags)) {
    return parent->Error("SILPass: Failed to read flags");
      // checks omitted
  }
  if (!table.ReadU8(&this->maxRuleLoop)) {
    return parent->Error("SILPass: Failed to read valid maxRuleLoop");
  }
  if (!table.ReadU8(&this->maxRuleContext)) {
    return parent->Error("SILPass: Failed to read maxRuleContext");
  }
  if (!table.ReadU8(&this->maxBackup)) {
    return parent->Error("SILPass: Failed to read maxBackup");
  }
  if (!table.ReadU16(&this->numRules)) {
    return parent->Error("SILPass: Failed to read numRules");
  }
  if (parent->version >> 16 >= 2) {
    if (!table.ReadU16(&this->fsmOffset)) {
      return parent->Error("SILPass: Failed to read fsmOffset");
    }
    if (parent->version >> 16 == 2 && this->fsmOffset != 0) {
      parent->Warning("SILPass: Nonzero fsmOffset (reserved in SILSub v2)");
    }
    if (!table.ReadU32(&this->pcCode) ||
        (parent->version >= 3 && this->pcCode < this->fsmOffset)) {
      return parent->Error("SILPass: Failed to read pcCode");
    }
  }
  if (!table.ReadU32(&this->rcCode) ||
      (parent->version >> 16 >= 2 && this->rcCode < this->pcCode)) {
    return parent->Error("SILPass: Failed to read valid rcCode");
  }
  if (!table.ReadU32(&this->aCode) || this->aCode < this->rcCode) {
    return parent->Error("SILPass: Failed to read valid aCode");
  }
  if (!table.ReadU32(&this->oDebug) ||
      (this->oDebug && this->oDebug < this->aCode)) {
    return parent->Error("SILPass: Failed to read valid oDebug");
  }
  if (parent->version >> 16 >= 3 &&
      table.offset() != init_offset + this->fsmOffset) {
    return parent->Error("SILPass: fsmOffset check failed");
  }
  if (!table.ReadU16(&this->numRows) ||
      (this->oDebug && this->numRows < this->numRules)) {
    return parent->Error("SILPass: Failed to read valid numRows");
  }
  if (!table.ReadU16(&this->numTransitional)) {
    return parent->Error("SILPass: Failed to read numTransitional");
  }
  if (!table.ReadU16(&this->numSuccess)) {
    return parent->Error("SILPass: Failed to read numSuccess");
  }
  if (!table.ReadU16(&this->numColumns)) {
    return parent->Error("SILPass: Failed to read numColumns");
  }
  if (!table.ReadU16(&this->numRange)) {
    return parent->Error("SILPass: Failed to read numRange");
  }

  // The following three fields are deprecated and ignored. We fix them up here
  // just for internal consistency, but the Graphite engine doesn't care.
  if (!table.ReadU16(&this->searchRange) ||
      !table.ReadU16(&this->entrySelector) ||
      !table.ReadU16(&this->rangeShift)) {
    return parent->Error("SILPass: Failed to read searchRange..rangeShift");
  }
  if (this->numRange == 0) {
    if (this->searchRange != 0 || this->entrySelector != 0 || this->rangeShift != 0) {
      this->searchRange = this->entrySelector = this->rangeShift = 0;
    }
  } else {
    unsigned floorLog2 = std::floor(std::log2(this->numRange));
    if (this->searchRange != 6 * (unsigned)std::pow(2, floorLog2) ||
        this->entrySelector != floorLog2 ||
        this->rangeShift != 6 * this->numRange - this->searchRange) {
      this->searchRange = 6 * (unsigned)std::pow(2, floorLog2);
      this->entrySelector = floorLog2;
      this->rangeShift = 6 * this->numRange - this->searchRange;
    }
  }

  //this->ranges.resize(this->numRange, parent);
  for (unsigned i = 0 ; i < this->numRange; ++i) {
    this->ranges.emplace_back(parent);
    if (!this->ranges[i].ParsePart(table)) {
      return parent->Error("SILPass: Failed to read ranges[%u]", i);
    }
  }
  unsigned ruleMap_len = 0;  // maximum value in oRuleMap
  //this->oRuleMap.resize(static_cast<unsigned long>(this->numSuccess) + 1);
  for (unsigned long i = 0; i <= this->numSuccess; ++i) {
    this->oRuleMap.emplace_back();
    if (!table.ReadU16(&this->oRuleMap[i])) {
      return parent->Error("SILPass: Failed to read oRuleMap[%u]", i);
    }
    if (oRuleMap[i] > ruleMap_len) {
      ruleMap_len = oRuleMap[i];
    }
  }

  //this->ruleMap.resize(ruleMap_len);
  for (unsigned i = 0; i < ruleMap_len; ++i) {
    this->ruleMap.emplace_back();
    if (!table.ReadU16(&this->ruleMap[i])) {
      return parent->Error("SILPass: Failed to read ruleMap[%u]", i);
    }
  }

  if (!table.ReadU8(&this->minRulePreContext)) {
    return parent->Error("SILPass: Failed to read minRulePreContext");
  }
  if (!table.ReadU8(&this->maxRulePreContext) ||
      this->maxRulePreContext < this->minRulePreContext) {
    return parent->Error("SILPass: Failed to read valid maxRulePreContext");
  }

  unsigned startStates_len = this->maxRulePreContext - this->minRulePreContext
                             + 1;
    // this->minRulePreContext <= this->maxRulePreContext
  //this->startStates.resize(startStates_len);
  for (unsigned i = 0; i < startStates_len; ++i) {
    this->startStates.emplace_back();
    if (!table.ReadS16(&this->startStates[i])) {
      return parent->Error("SILPass: Failed to read startStates[%u]", i);
    }
  }

  //this->ruleSortKeys.resize(this->numRules);
  for (unsigned i = 0; i < this->numRules; ++i) {
    this->ruleSortKeys.emplace_back();
    if (!table.ReadU16(&this->ruleSortKeys[i])) {
      return parent->Error("SILPass: Failed to read ruleSortKeys[%u]", i);
    }
  }

  //this->rulePreContext.resize(this->numRules);
  for (unsigned i = 0; i < this->numRules; ++i) {
    this->rulePreContext.emplace_back();
    if (!table.ReadU8(&this->rulePreContext[i])) {
      return parent->Error("SILPass: Failed to read rulePreContext[%u]", i);
    }
  }

  if (parent->version >> 16 >= 2) {
    if (!table.ReadU8(&this->collisionThreshold)) {
      return parent->Error("SILPass: Failed to read collisionThreshold");
    }
    if (parent->version >> 16 < 5 && this->collisionThreshold != 0) {
      parent->Warning("SILPass: Nonzero collisionThreshold"
                      " (reserved before v5)");
    }
    if (!table.ReadU16(&this->pConstraint)) {
      return parent->Error("SILPass: Failed to read pConstraint");
    }
  }

  unsigned long ruleConstraints_len = this->aCode - this->rcCode;
    // this->rcCode <= this->aCode
  //this->oConstraints.resize(static_cast<unsigned long>(this->numRules) + 1);
  for (unsigned long i = 0; i <= this->numRules; ++i) {
    this->oConstraints.emplace_back();
    if (!table.ReadU16(&this->oConstraints[i]) ||
        this->oConstraints[i] > ruleConstraints_len) {
      return parent->Error("SILPass: Failed to read valid oConstraints[%lu]",
                           i);
    }
  }

  if (!this->oDebug && this->aCode > next_pass_offset) {
    return parent->Error("SILPass: Failed to calculate length of actions");
  }
  unsigned long actions_len = this->oDebug ? this->oDebug - this->aCode :
                                             next_pass_offset - this->aCode;
    // if this->oDebug, then this->aCode <= this->oDebug
  //this->oActions.resize(static_cast<unsigned long>(this->numRules) + 1);
  for (unsigned long i = 0; i <= this->numRules; ++i) {
    this->oActions.emplace_back();
    if (!table.ReadU16(&this->oActions[i]) ||
        (this->oActions[i] > actions_len)) {
      return parent->Error("SILPass: Failed to read valid oActions[%lu]", i);
    }
  }

  //this->stateTrans.resize(this->numTransitional);
  for (unsigned i = 0; i < this->numTransitional; ++i) {
    this->stateTrans.emplace_back();
    //this->stateTrans[i].resize(this->numColumns);
    for (unsigned j = 0; j < this->numColumns; ++j) {
      this->stateTrans[i].emplace_back();
      if (!table.ReadU16(&stateTrans[i][j])) {
        return parent->Error("SILPass: Failed to read stateTrans[%u][%u]",
                             i, j);
      }
    }
  }

  if (parent->version >> 16 >= 2) {
    if (!table.ReadU8(&this->reserved2)) {
      return parent->Error("SILPass: Failed to read reserved2");
    }
    if (this->reserved2 != 0) {
      parent->Warning("SILPass: Nonzero reserved2");
    }

    if (table.offset() != SILSub_init_offset + this->pcCode) {
      return parent->Error("SILPass: pcCode check failed");
    }
    //this->passConstraints.resize(this->pConstraint);
    for (unsigned i = 0; i < this->pConstraint; ++i) {
      this->passConstraints.emplace_back();
      if (!table.ReadU8(&this->passConstraints[i])) {
        return parent->Error("SILPass: Failed to read passConstraints[%u]", i);
      }
    }
  }

  if (table.offset() != SILSub_init_offset + this->rcCode) {
    return parent->Error("SILPass: rcCode check failed");
  }
  //this->ruleConstraints.resize(ruleConstraints_len);  // calculated above
  for (unsigned long i = 0; i < ruleConstraints_len; ++i) {
    this->ruleConstraints.emplace_back();
    if (!table.ReadU8(&this->ruleConstraints[i])) {
      return parent->Error("SILPass: Failed to read ruleConstraints[%u]", i);
    }
  }

  if (table.offset() != SILSub_init_offset + this->aCode) {
    return parent->Error("SILPass: aCode check failed");
  }
  //this->actions.resize(actions_len);  // calculated above
  for (unsigned long i = 0; i < actions_len; ++i) {
    this->actions.emplace_back();
    if (!table.ReadU8(&this->actions[i])) {
      return parent->Error("SILPass: Failed to read actions[%u]", i);
    }
  }

  if (this->oDebug) {
    OpenTypeNAME* name = static_cast<OpenTypeNAME*>(
        parent->GetFont()->GetTypedTable(OTS_TAG_NAME));
    if (!name) {
      return parent->Error("SILPass: Required name table is missing");
    }

    if (table.offset() != SILSub_init_offset + this->oDebug) {
      return parent->Error("SILPass: oDebug check failed");
    }
    //this->dActions.resize(this->numRules);
    for (unsigned i = 0; i < this->numRules; ++i) {
      this->dActions.emplace_back();
      if (!table.ReadU16(&this->dActions[i]) ||
          !name->IsValidNameId(this->dActions[i])) {
        return parent->Error("SILPass: Failed to read valid dActions[%u]", i);
      }
    }

    unsigned dStates_len = this->numRows - this->numRules;
      // this->numRules <= this->numRows
    //this->dStates.resize(dStates_len);
    for (unsigned i = 0; i < dStates_len; ++i) {
      this->dStates.emplace_back();
      if (!table.ReadU16(&this->dStates[i]) ||
          !name->IsValidNameId(this->dStates[i])) {
        return parent->Error("SILPass: Failed to read valid dStates[%u]", i);
      }
    }

    //this->dCols.resize(this->numRules);
    for (unsigned i = 0; i < this->numRules; ++i) {
      this->dCols.emplace_back();
      if (!table.ReadU16(&this->dCols[i]) ||
          !name->IsValidNameId(this->dCols[i])) {
        return parent->Error("SILPass: Failed to read valid dCols[%u]");
      }
    }
  }
  return true;
}

bool OpenTypeSILF::SILSub::
SILPass::SerializePart(OTSStream* out) const {
  if (!out->WriteU8(this->flags) ||
      !out->WriteU8(this->maxRuleLoop) ||
      !out->WriteU8(this->maxRuleContext) ||
      !out->WriteU8(this->maxBackup) ||
      !out->WriteU16(this->numRules) ||
      (parent->version >> 16 >= 2 &&
       (!out->WriteU16(this->fsmOffset) ||
        !out->WriteU32(this->pcCode))) ||
      !out->WriteU32(this->rcCode) ||
      !out->WriteU32(this->aCode) ||
      !out->WriteU32(this->oDebug) ||
      !out->WriteU16(this->numRows) ||
      !out->WriteU16(this->numTransitional) ||
      !out->WriteU16(this->numSuccess) ||
      !out->WriteU16(this->numColumns) ||
      !out->WriteU16(this->numRange) ||
      !out->WriteU16(this->searchRange) ||
      !out->WriteU16(this->entrySelector) ||
      !out->WriteU16(this->rangeShift) ||
      !SerializeParts(this->ranges, out) ||
      !SerializeParts(this->oRuleMap, out) ||
      !SerializeParts(this->ruleMap, out) ||
      !out->WriteU8(this->minRulePreContext) ||
      !out->WriteU8(this->maxRulePreContext) ||
      !SerializeParts(this->startStates, out) ||
      !SerializeParts(this->ruleSortKeys, out) ||
      !SerializeParts(this->rulePreContext, out) ||
      (parent->version >> 16 >= 2 &&
       (!out->WriteU8(this->collisionThreshold) ||
        !out->WriteU16(this->pConstraint))) ||
      !SerializeParts(this->oConstraints, out) ||
      !SerializeParts(this->oActions, out) ||
      !SerializeParts(this->stateTrans, out) ||
      (parent->version >> 16 >= 2 &&
       (!out->WriteU8(this->reserved2) ||
        !SerializeParts(this->passConstraints, out))) ||
      !SerializeParts(this->ruleConstraints, out) ||
      !SerializeParts(this->actions, out) ||
      !SerializeParts(this->dActions, out) ||
      !SerializeParts(this->dStates, out) ||
      !SerializeParts(this->dCols, out)) {
    return parent->Error("SILPass: Failed to write");
  }
  return true;
}

bool OpenTypeSILF::SILSub::SILPass::
PassRange::ParsePart(Buffer& table) {
  if (!table.ReadU16(&this->firstId)) {
    return parent->Error("PassRange: Failed to read firstId");
  }
  if (!table.ReadU16(&this->lastId)) {
    return parent->Error("PassRange: Failed to read lastId");
  }
  if (!table.ReadU16(&this->colId)) {
    return parent->Error("PassRange: Failed to read colId");
  }
  return true;
}

bool OpenTypeSILF::SILSub::SILPass::
PassRange::SerializePart(OTSStream* out) const {
  if (!out->WriteU16(this->firstId) ||
      !out->WriteU16(this->lastId) ||
      !out->WriteU16(this->colId)) {
    return parent->Error("PassRange: Failed to write");
  }
  return true;
}

}  // namespace ots
